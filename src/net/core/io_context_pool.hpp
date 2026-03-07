#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

namespace warp::net::core {

class io_context_pool {
public:
	explicit io_context_pool(std::size_t pool_size = std::thread::hardware_concurrency());
	io_context_pool(const io_context_pool &) = delete;
	io_context_pool &operator=(const io_context_pool &) = delete;
	io_context_pool(io_context_pool &&) = delete;
	io_context_pool &operator=(io_context_pool &&) = delete;
	~io_context_pool();

	[[nodiscard]] boost::asio::io_context &next();
	void run();
	void stop();

private:
	using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

	std::vector<std::unique_ptr<boost::asio::io_context>> contexts_;
	std::vector<std::unique_ptr<work_guard>> guards_;
	std::vector<std::thread> threads_;
	std::atomic<std::size_t> next_ {0};
	std::atomic<bool> running_ {false};
};

} // namespace warp::net::core
