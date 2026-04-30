#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core.hpp>

#include "http_listener.h"
#include "server/router/registry.hpp"

namespace warp::server {

class coroutine_listener final
    : public http_listener<coroutine_listener, route_executor_table<http::event_loop_mode::coroutines>>,
      public std::enable_shared_from_this<coroutine_listener> {
public:
	coroutine_listener(boost::asio::io_context &ioc, const registry &registry,
	                   const route_executor_table<http::event_loop_mode::coroutines> &route_executors,
	                   const interceptor_chain<request> &req_interceptor_chain,
	                   const interceptor_chain<response> &resp_interceptor_chain, const std::string &address,
	                   unsigned short port, log::logger logger);

	void execute();

private:
	boost::asio::awaitable<void> accept_loop();
};

} // namespace warp::server
