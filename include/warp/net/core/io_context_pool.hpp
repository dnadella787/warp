#pragma once

#include <memory>
#include <thread>

namespace warp::net::core {

namespace detail {
struct executor_access;
} // namespace detail

class io_context_pool {
public:
    explicit io_context_pool(std::size_t pool_size = std::thread::hardware_concurrency());
    io_context_pool(const io_context_pool&) = delete;
    io_context_pool& operator=(const io_context_pool&) = delete;
    io_context_pool(io_context_pool&&) noexcept;
    io_context_pool& operator=(io_context_pool&&) noexcept;
    ~io_context_pool();

    class executor {
    public:
        executor() = default;
        executor(const executor&) = default;
        executor& operator=(const executor&) = default;
        executor(executor&&) noexcept = default;
        executor& operator=(executor&&) noexcept = default;
        ~executor() = default;

        [[nodiscard]] bool valid() const noexcept { return context_ != nullptr; }

    private:
        friend class io_context_pool;
        friend struct detail::executor_access;

        explicit executor(void* context) noexcept
            : context_(context) {}

        void* context_{nullptr};
    };

    [[nodiscard]] executor next_executor();
    void run();
    void stop();

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace warp::net::core
