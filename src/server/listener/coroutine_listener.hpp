#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>

#include "http_listener.h"
#include "server/router/registry.hpp"

namespace warp::server {

template <warp_session_transport Transport>
class coroutine_listener final : public listener_base<event_loop_mode::coroutines, Transport>,
                                 public std::enable_shared_from_this<coroutine_listener<Transport>> {
public:
	using base_type = listener_base<event_loop_mode::coroutines, Transport>;

	coroutine_listener(boost::asio::io_context &ioc, transport_provider<Transport> transport_provider,
	                   const registry &registry, const std::vector<http::handler> &route_handlers,
	                   const interceptor_chain<request> &req_interceptor_chain,
	                   const interceptor_chain<response> &resp_interceptor_chain, const std::string &address,
	                   unsigned short port, log::logger logger);

	void run() override;

private:
	boost::asio::awaitable<void> accept_loop();
};

} // namespace warp::server
