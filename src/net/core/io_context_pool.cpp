#include "io_context_pool.hpp"

#include <boost/asio/strand.hpp>

namespace warp::net::core {

io_context_pool::io_context_pool(std::size_t pool_size) {
    if (pool_size == 0) {
	pool_size = 1;
    }
    contexts_.reserve(pool_size);
    guards_.reserve(pool_size);
    for (std::size_t i = 0; i < pool_size; ++i) {
	auto ctx = std::make_unique<boost::asio::io_context>();
	guards_.push_back(std::make_unique<work_guard>(boost::asio::make_work_guard(*ctx)));
	contexts_.push_back(std::move(ctx));
    }
}

io_context_pool::~io_context_pool() {
    stop();
}

boost::asio::io_context &io_context_pool::next() {
    auto idx = next_.fetch_add(1, std::memory_order_relaxed);
    return *contexts_[idx % contexts_.size()];
}

void io_context_pool::run() {
    if (running_.exchange(true)) {
	return;
    }
    threads_.reserve(contexts_.size());
    for (auto &ctx : contexts_) {
	threads_.emplace_back([&ctx = *ctx]() { ctx.run(); });
    }
}

void io_context_pool::stop() {
    if (!running_.exchange(false)) {
	return;
    }
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