#include "io_context_pool.hpp"

namespace warp::net::core {

io_context_pool::io_context_pool(std::size_t pool_size) {
	if (pool_size == 0) {
		pool_size = 1;
	}
	contexts_.reserve(pool_size);
	guards_.reserve(pool_size);
	for (std::size_t i = 0; i < pool_size; ++i) {
		auto ctx = std::make_unique<boost::asio::io_context>();
		// use work guards so that ctx.run() doesn't exit when there are no handlers queued up
		guards_.push_back(std::make_unique<work_guard>(boost::asio::make_work_guard(*ctx)));
		contexts_.push_back(std::move(ctx));
	}
}

io_context_pool::~io_context_pool() {
	stop();
}

boost::asio::io_context &io_context_pool::next() {
	// use std::memory_order_relaxed bc we dont need acquire/release since next_ does
	// not indicate the state of any other variable to calling threads, it just returns
	// the next boost::asio::io_context in a round robin manner. All we need is an
	// increment of next_ in a single indivisible step.
	//
	// std::size_t guarantees wrap around overflow behavior so we don't have to worry
	// about the size_t max value. Plus the max is like 2^64 so we need billions of
	// requests to reach the wrap around... although a wrap around may cause next() to
	// repeat ctx w/o a full loop if 2^64 - 1 % contexts_.size() != contexts_.size() - 1
	//
	// Also note that we do not check whether is pool has started up or not since it is
	// possible to create tasks on the io_contexts without starting them. The tasks would
	// just execute after they are started.
	auto idx = next_.fetch_add(1, std::memory_order_relaxed);
	return *contexts_[idx % contexts_.size()];
}

void io_context_pool::run() {
	// try to start, if its already running just return early, use acq_rel
	// b/c we want to acquire the current state and check if it in a non-running
	// state, and then release it to consumers like run()/stop() on other threads
	if (running_.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	threads_.reserve(contexts_.size());
	for (auto &ctx : contexts_) {
		threads_.emplace_back([&ctx = *ctx]() { ctx.run(); });
	}
}

void io_context_pool::stop() {
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
	// completed and the acceptor is closed first in the server_impl class (so no more
	// request handlers can be queued up). It will increase server shutdown time and may
	// cause deadlock if a request queues up more async_accept's but provides a better
	for (auto &guard : guards_) {
		guard->reset();
	}
	for (auto &ctx : contexts_) {
		ctx->stop();
	}
	for (auto &t : threads_) {
		if (t.joinable()) {
			t.join();
		}
	}
	threads_.clear();
}

} // namespace warp::net::core