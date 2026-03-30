#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "listener/listener_base.hpp"
#include "router/registry.hpp"
#include "warp/http/server.hpp"

namespace warp::http {

class server::impl : public std::enable_shared_from_this<impl> {
public:
	impl(const std::string &address, std::uint16_t port, std::size_t workers, event_loop_mode mode, registry routes);

	void run(bool blocking = true);
	void stop();

private:
	void start_runner_threads();
	void stop_io_ctx();

	std::size_t pool_size_;
	boost::asio::io_context io_ctx_;
	registry routes_;
	std::shared_ptr<listener_base> listener_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
	std::vector<std::thread> threads_;
	std::atomic<bool> running_ {false};
};

} // namespace warp::http
