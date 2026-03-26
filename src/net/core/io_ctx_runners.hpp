#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

namespace warp::net::core {

using namespace boost::asio;

class io_ctx_runners {
public:
	explicit io_ctx_runners(std::shared_ptr<io_context> io_context, std::size_t pool_size);
	io_ctx_runners(const io_ctx_runners &) = delete;
	io_ctx_runners &operator=(const io_ctx_runners &) = delete;
	io_ctx_runners(io_ctx_runners &&) = delete;
	io_ctx_runners &operator=(io_ctx_runners &&) = delete;
	~io_ctx_runners();

	[[nodiscard]] io_context &get() const noexcept;
	void run();
	void stop();

private:
	std::size_t pool_size_;
	std::shared_ptr<io_context> io_ctx_;
	std::unique_ptr<executor_work_guard<io_context::executor_type>> guard_;
	std::vector<std::thread> threads_;
	std::atomic<bool> running_ {false};
};

} // namespace warp::net::core
