//
// Created by Dhanush Nadella on 4/11/26.
//

#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

#include <boost/asio/ip/tcp.hpp>

#include "server/execution/route_executor_table.hpp"
#include "base_listener.hpp"
#include "server/interceptors/interceptor_chain.h"
#include "server/router/registry.hpp"
#include "server/session/policy/transport.h"
#include "server/session/policy/transport_provider.h"
#include "warp/logging/logger.hpp"

namespace warp::server {

template <http::event_loop_mode Mode, warp_session_transport Transport>
struct event_loop_traits;

template <http::event_loop_mode Mode, warp_session_transport Transport>
class listener_base : public base_listener {
public:
	using session_type = typename event_loop_traits<Mode, Transport>::session_type;
	using executor_table_t = typename event_loop_traits<Mode, Transport>::executor_table_type;

	listener_base(boost::asio::io_context &ioc, transport_provider<Transport> transport_provider,
	              const registry &registry, const std::vector<http::handler> &route_handlers,
	              const interceptor_chain<request> &req_interceptor_chain,
	              const interceptor_chain<response> &resp_interceptor_chain, const std::string &address,
	              unsigned short port, log::logger logger);

protected:
	void start_session(boost::asio::ip::tcp::socket socket);

	boost::asio::io_context &ioc_;
	boost::asio::ip::tcp::acceptor acceptor_;
	[[no_unique_address]] transport_provider<Transport> transport_provider_;
	executor_table_t route_executors_;
	const registry &registry_;
	const interceptor_chain<request> &req_interceptor_chain_;
	const interceptor_chain<response> &resp_interceptor_chain_;
	log::logger logger_;
};

} // namespace warp::server

#include "http_listener.tpp"
