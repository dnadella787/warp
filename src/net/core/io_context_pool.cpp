#include "warp/net/core/io_context_pool.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace warp::net::core {

namespace {
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
} // namespace

class io_context_pool::impl {
public:
    explicit impl(std::size_t pool_size) {
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

    ~impl() {
        stop();
    }

    io_context_pool::executor next_executor() {
        auto idx = next_.fetch_add(1, std::memory_order_relaxed);
        auto* ctx = contexts_[idx % contexts_.size()].get();
        return io_context_pool::executor(static_cast<void*>(ctx));
    }

    void run() {
        if (running_.exchange(true)) {
            return;
        }
        threads_.reserve(contexts_.size());
        for (auto& ctx : contexts_) {
            threads_.emplace_back([context = ctx.get()]() { context->run(); });
        }
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        for (auto& guard : guards_) {
            guard->reset();
        }
        for (auto& ctx : contexts_) {
            ctx->stop();
        }
        for (auto& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        threads_.clear();
    }

private:
    std::vector<std::unique_ptr<boost::asio::io_context>> contexts_;
    std::vector<std::unique_ptr<work_guard>> guards_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> next_{0};
    std::atomic<bool> running_{false};
};

io_context_pool::io_context_pool(std::size_t pool_size)
    : impl_(std::make_unique<impl>(pool_size)) {}

io_context_pool::io_context_pool(io_context_pool&&) noexcept = default;

io_context_pool& io_context_pool::operator=(io_context_pool&&) noexcept = default;

io_context_pool::~io_context_pool() = default;

io_context_pool::executor io_context_pool::next_executor() {
    return impl_->next_executor();
}

void io_context_pool::run() {
    impl_->run();
}

void io_context_pool::stop() {
    impl_->stop();
}

} // namespace warp::net::core
