#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

namespace warp::net::core {

using namespace boost::asio;

class io_context_pool {
public:
	explicit io_context_pool(std::size_t pool_size = std::thread::hardware_concurrency());
	io_context_pool(const io_context_pool &) = delete;
	io_context_pool &operator=(const io_context_pool &) = delete;
	io_context_pool(io_context_pool &&) = delete;
	io_context_pool &operator=(io_context_pool &&) = delete;
	~io_context_pool();

	[[nodiscard]] io_context &get() const noexcept;
	void run();
	void stop();

private:
	std::size_t pool_size_;
	std::unique_ptr<io_context> ioctx_;
	std::unique_ptr<executor_work_guard<io_context::executor_type>> guard_;
	std::vector<std::thread> threads_;
	std::atomic<bool> running_ {false};
};

} // namespace warp::net::core
