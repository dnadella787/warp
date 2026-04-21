#pragma once
#include "server_impl.hpp"

#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>


namespace warp::http {

template <event_loop_mode Mode>
server::server_impl<Mode>::server_impl(const std::string &address, std::uint16_t port, std::size_t workers, registry routes)
: pool_size_(workers ? workers : 1), io_ctx_(static_cast<int>(pool_size_)), routes_(std::move(routes)),
	listener_(std::make_shared<listener_t>(io_ctx_, routes_, address, port)) {
	threads_.reserve(pool_size_);
}

template <event_loop_mode _>
void server::server_impl<_>::run(bool blocking) {
	std::vector<std::thread> threads_to_join;

	{
		std::unique_lock lock(lifecycle_mutex_);
		if (state_ != lifecycle_state::stopped) {
			return;
		}

		state_ = lifecycle_state::starting;
		io_ctx_.restart(); // this is a noop on the first try, otherwise it actually resets the io_ctx for reuse
		guard_.emplace(boost::asio::make_work_guard(io_ctx_));

		try {
			start_runner_threads();
			listener_->run();
			state_ = lifecycle_state::running;
		} catch (...) {
			state_ = lifecycle_state::stopping;
			stopping_thread_id_ = std::this_thread::get_id();
			threads_to_join = stop_io_ctx();
			lock.unlock();
			join_runner_threads(std::move(threads_to_join), std::this_thread::get_id());
			lock.lock();
			state_ = lifecycle_state::stopped;
			stopping_thread_id_.reset();
			lock.unlock();
			lifecycle_cv_.notify_all();
			throw;
		}
	}

	if (!blocking) {
		return;
	}

	try {
		io_ctx_.run();
	} catch (...) {
		stop();
		throw;
	}
}

template <event_loop_mode Mode>
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

template <event_loop_mode Mode>
void server::server_impl<Mode>::start_runner_threads() {
	for (std::size_t i = 0; i < pool_size_; i++) {
		threads_.emplace_back([&ctx = io_ctx_]() {
			for (;;) {
				try {
					ctx.run();
					break;
				} catch (const std::exception &ex) {
					std::cerr << "worker error: " << ex.what() << std::endl;
				}
			}
		});
	}
}

template <event_loop_mode Mode>
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

template <event_loop_mode Mode>
void server::server_impl<Mode>::join_runner_threads(std::vector<std::thread> threads, std::thread::id current_thread_id) {
	for (auto &t : threads) {
		if (t.joinable()) {
			if (t.get_id() == current_thread_id) {
				t.detach();
				continue;
			}
			t.join();
		}
	}
}

} // namespace warp::http
