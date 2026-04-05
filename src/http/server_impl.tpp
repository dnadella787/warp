#pragma once
#include "server_impl.hpp"

#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>


namespace warp::http {

template <event_loop_mode Mode>
server::server_impl<Mode>::server_impl(const std::string &address, std::uint16_t port, std::size_t workers, registry routes)
: pool_size_(workers ? workers : 1), io_ctx_(static_cast<int>(pool_size_)), routes_(std::move(routes)),
	guard_(boost::asio::make_work_guard(io_ctx_)), listener_(std::make_shared<listener_t>(io_ctx_, routes_, address, port)) {
	threads_.reserve(pool_size_);
}

/*
 std::atomic<bool> running_ checked via std::atomic::exchange(true/false, std::memory_order_acq_rel)
 guarantees that run()/stop() check the value and update the server resources (like thread pool/listener)
 accordingly before propagating the change downstream to an acquire on another thread (e.g. run()/stop()).

 TLDR: run/stop are r/w
 */
template <event_loop_mode _>
void server::server_impl<_>::run(bool blocking) {
	// try to start, if its already running just return early, use acq_rel
	// b/c we want to acquire the current state and check if it in a non-running
	// state, and then release it to consumers like listener::run()/stop()
	if (running_.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	start_runner_threads();
	listener_->run();
	if (blocking)
		io_ctx_.run();
}

template <event_loop_mode Mode>
void server::server_impl<Mode>::stop() {
	// try to stop, if its already stopped just return early, use acq_rel
	// b/c we want to ensure that all previous activity on running_ is
	// published (i.e. from run() or other stop() threads) before we cancel
	// the io_context pool and acceptor
	if (!running_.exchange(false, std::memory_order_acq_rel)) {
		return;
	}

	stop_io_ctx();
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
void server::server_impl<Mode>::stop_io_ctx() {
	guard_.reset();
	io_ctx_.stop();
	for (auto &t : threads_) {
		if (t.joinable()) {
			t.join();
		}
	}
	threads_.clear();
}

} // namespace warp::http
