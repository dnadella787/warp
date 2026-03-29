#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "listener.h"
#include "warp/http/server.hpp"
#include "../../net/router/registry.hpp"

namespace warp::http {

class server::impl : public std::enable_shared_from_this<impl> {
public:
	impl(const std::string &address, std::uint16_t port, std::size_t workers, const net::router::registry &routes);

	void run();
	void stop();

private:
	void start_runner_threads();
	void stop_io_ctx();

	std::size_t pool_size_;
	boost::asio::io_context io_ctx_;
	detail::listener listener_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
	std::vector<std::thread> threads_;

	net::router::registry routes_;
	std::atomic<bool> running_ {false};
};

} // namespace warp::http
