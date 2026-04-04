#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio/ip/tcp.hpp>
#include "listener/listener_base.hpp"
#include "listener/traits.hpp"
#include "router/registry.hpp"
#include "warp/http/server.hpp"

namespace warp::http {

template <event_loop_mode Mode>
class server::server_impl : public std::enable_shared_from_this<server_impl<Mode>>, public impl_base {
public:
	server_impl(const std::string &address, std::uint16_t port, std::size_t workers, registry routes);

	void run(bool blocking);
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

	using listener_t = typename listener_traits<Mode>::type;
};
} // namespace warp::http

#include "server_impl.tpp"