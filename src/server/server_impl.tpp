#pragma once
#include "server_impl.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

namespace warp::server {

template <http::event_loop_mode Mode>
server::server_impl<Mode>::server_impl(const std::string &address, std::uint16_t port, std::size_t workers,
                                       registry routes, route_executor_table<Mode> route_executors,
                                       interceptor_chain<request> req_interceptor_chain,
                                       interceptor_chain<response> resp_interceptor_chain, log::logger logger)
    : pool_size_(workers ? workers : 1), io_ctx_(static_cast<int>(pool_size_)), routes_(std::move(routes)),
      route_executors_(std::move(route_executors)), req_interceptor_chain_(std::move(req_interceptor_chain)),
      resp_interceptor_chain_(std::move(resp_interceptor_chain)),
      logger_(std::move(logger)),
      listener_(std::make_shared<listener_t>(io_ctx_, routes_, route_executors_, req_interceptor_chain_,
                                             resp_interceptor_chain_, address, port, logger_)) {
	threads_.reserve(pool_size_);
}

template <http::event_loop_mode _>
void server::server_impl<_>::run(bool blocking) {
	std::vector<std::thread> threads_to_join;

	{
		std::unique_lock lock(lifecycle_mutex_);
		if (state_ != lifecycle_state::stopped) {
			return;
		}

		state_ = lifecycle_state::starting;
		io_ctx_.restart(); // this is a noop on the first try, otherwise it actually resets the io_ctx for reuse (resets `stopped` flag internally)
		guard_.emplace(boost::asio::make_work_guard(io_ctx_)); // create a new guard each time you start the server

		try {
			start_runner_threads();
			listener_->run();
			state_ = lifecycle_state::running;
		} catch (...) {
			state_ = lifecycle_state::stopping;
			stopping_thread_id_ = std::this_thread::get_id();
			threads_to_join = stop_io_ctx();
			lock.unlock();
			/*
			 * join() can block for a while so we unlock the mutex. Otherwise, if you keep lifecycle_mutex_ held
			 * during the entire join, every concurrent stop() call blocks at the mutex instead of seeing state_
			 * == stopping and waiting cleanly on lifecycle_cv_.
			 *
	   		 * Also prevents deadlock if a worker thread being joined tries to call stop(). That worker would try
	   		 * to acquire lifecycle_mutex_ in stop(), but this thread is holding the mutex but is also waiting
	   		 * for that worker to finish during the join().
	   		 *
	   		 * Honestly user should not be doing this but if they decide to put server->stop() within a request handler
	   		 * or resource destructor somehow I guess this protects them
	   		 *
			 * we need to reacquire the lock afterwards to complete the state transition
			 */
			join_runner_threads(std::move(threads_to_join), std::this_thread::get_id());
			lock.lock();
			state_ = lifecycle_state::stopped;
			stopping_thread_id_.reset();
			lock.unlock();
			// unlock then notify waiters so that they do not go wake up see the mutex is still held and
			// just go back to sleep until their next spurious wakeup at which point they recheck the
			// state and exit
			lifecycle_cv_.notify_all();
			throw;
		}
	}

	if (!blocking)
		return;

	try {
		io_ctx_.run();
	} catch (...) {
		logger_.error("Error in io_context::run() on main blocking thread for server_impl::run(), stopping server");
		stop();
		throw;
	}
}

template <http::event_loop_mode Mode>
void server::server_impl<Mode>::stop() {
	std::vector<std::thread> threads_to_join;

	{
		std::unique_lock lock(lifecycle_mutex_);
		// server is already stopped by another caller, so just exit
		if (state_ == lifecycle_state::stopped)
			return;

		// server is currently in the process of stopping due to another caller
		if (state_ == lifecycle_state::stopping) {
			// if the thread that is currently stopping the server is the same as this
			// instance of the stop call, then exit immediately, although this technically should
			// not be possible? unless a request handler calls server::stop() which triggers
			// another stop via implicit destructor call
			if (stopping_thread_id_ == std::this_thread::get_id())
				return;

			// wait till the stopping thread notifies this thread for when
			// the state is stopped (it will also spuriously wake up and go to sleep/move on
			// based on the state moving to stopped or not)
			lifecycle_cv_.wait(lock, [this]() {
				return state_ == lifecycle_state::stopped;
			});
			return;
		}

		state_ = lifecycle_state::stopping;
		stopping_thread_id_ = std::this_thread::get_id();
		threads_to_join = stop_io_ctx();
	}
	
	join_runner_threads(std::move(threads_to_join), stopping_thread_id_.value());

	{
		std::lock_guard lock(lifecycle_mutex_);
		state_ = lifecycle_state::stopped;
		stopping_thread_id_.reset();
	}
	// release the lock before notifying so that waiting threads don't wake up and just
	// go back to sleep
	lifecycle_cv_.notify_all();
}

template <http::event_loop_mode Mode>
void server::server_impl<Mode>::start_runner_threads() {
	for (std::size_t i = 0; i < pool_size_; i++) {
		threads_.emplace_back([&ctx = io_ctx_, logger = logger_]() {
			for (;;) {
				try {
					ctx.run();
					break;
				} catch (const std::exception &ex) {
					logger.error("Worker error: {}", ex.what());
				}
			}
		});
	}
}

template <http::event_loop_mode Mode>
std::vector<std::thread> server::server_impl<Mode>::stop_io_ctx() {
	if (guard_) {
		guard_->reset();
		guard_.reset();
	}

	io_ctx_.stop();

	auto threads = std::move(threads_);
	threads_.reserve(pool_size_);
	return threads;
}

template <http::event_loop_mode Mode>
void server::server_impl<Mode>::join_runner_threads(std::vector<std::thread> threads, std::thread::id current_thread_id) {
	for (auto &t : threads) {
		if (t.joinable()) {
			// t.join on itself would mean thread is waiting for itself to exit which can never happen...
			if (t.get_id() == current_thread_id) {
				t.detach(); // so we detach the thread and let it terminate itself (io_ctx.stop() called already and work guard removed)
				continue;
			}
			t.join();
		}
	}
}

} // namespace warp::server
