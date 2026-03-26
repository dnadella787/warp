#include "io_ctx_runners.hpp"

#include <iostream>

namespace warp::net::core {

io_ctx_runners::io_ctx_runners(std::shared_ptr<io_context> io_context, std::size_t pool_size) {
	if (pool_size == 0) {
		pool_size = 1;
	}
	// provide concurrency hint so io_context is aware of the underlying set of threads
	// instead of having to wrangle with multiple io_context objects ourselves
	pool_size_ = pool_size;
	io_ctx_ = std::move(io_context);
	guard_ = std::make_unique<executor_work_guard<io_context::executor_type>>(make_work_guard(*io_ctx_));
	threads_.reserve(pool_size);
}

io_ctx_runners::~io_ctx_runners() {
	stop();
}

io_context &io_ctx_runners::get() const noexcept {
	return *io_ctx_;
}

void io_ctx_runners::run() {
	// try to start, if its already running just return early, use acq_rel
	// b/c we want to acquire the current state and check if it in a non-running
	// state, and then release it to consumers like run()/stop() on other threads
	if (running_.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	for (std::size_t i = 0; i < pool_size_; i++) {
		threads_.emplace_back([&ctx = *io_ctx_]() {
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

void io_ctx_runners::stop() {
	// try to stop, if its already stopped just return early, use acq_rel
	// b/c we want to ensure that all previous activity on running_ is
	// published (i.e. from run() or other stop() threads) before we cancel
	// the contexts and threads
	if (!running_.exchange(false, std::memory_order_acq_rel)) {
		return;
	}
	// Remove work guards even though technically io_context::stop() kills the ctx
	// immediately and forces it to exit once the current handler is completed
	//
	// TODO: Maybe skip ctx->stop() and just just do guard->reset() and t.join() since
	// removing the guard will make the ctx.run() exit once all queued up tasks are
	// completed since the acceptor is closed first in the server_impl class (so no more
	// request handlers can be queued up). It will increase server shutdown time and may
	// cause deadlock if a request queues up more async_accept's but provides a graceful
	// shutdown experience
	guard_->reset();
	io_ctx_->stop();
	for (auto &t : threads_) {
		if (t.joinable()) {
			t.join();
		}
	}
	threads_.clear();
}

} // namespace warp::net::core