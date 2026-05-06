//
// Created by Dhanush Nadella on 4/5/26.
//
#include "http_listener.h"

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>

#include "traits.hpp"

namespace warp::server {

namespace beast = boost::beast;

template <http::event_loop_mode Mode, warp_session_transport Transport>
listener_base<Mode, Transport>::listener_base(boost::asio::io_context &ioc,
                                              transport_provider<Transport> transport_provider,
                                              const route_runtime_t &routes,
                                              const interceptor_chain<request> &req_interceptor_chain,
                                              const interceptor_chain<response> &resp_interceptor_chain,
                                              const std::string &address, const unsigned short port,
                                              log::logger logger)
	: ioc_(ioc), acceptor_(boost::asio::make_strand(ioc)),
	  transport_provider_(std::move(transport_provider)), routes_(routes),
	  req_interceptor_chain_(req_interceptor_chain),
	  resp_interceptor_chain_(resp_interceptor_chain), logger_(std::move(logger)) {
	auto const addr = boost::asio::ip::make_address(address);
	auto const endpoint = boost::asio::ip::tcp::endpoint {addr, port};
	beast::error_code ec;

	acceptor_.open(endpoint.protocol(), ec);
	if (ec) {
		logger_.error("error in base_listener during open: {}", ec.message());
		throw std::runtime_error(ec.message());
	}

	acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec) {
		logger_.error("error in base_listener during set_option{{reuse_address=true}}: {}", ec.message());
		throw std::runtime_error(ec.message());
	}

	acceptor_.bind(endpoint, ec);
	if (ec) {
		logger_.error("error in base_listener during bind: {}", ec.message());
		throw std::runtime_error(ec.message());
	}

	acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
	if (ec) {
		logger_.error("error in base_listener during listen: {}", ec.message());
		throw std::runtime_error(ec.message());
	}
}

template <http::event_loop_mode Mode, warp_session_transport Transport>
void listener_base<Mode, Transport>::start_session(boost::asio::ip::tcp::socket socket) {
	std::make_shared<session_type>(std::move(socket), transport_provider_.make_transport(), routes_,
	                               req_interceptor_chain_, resp_interceptor_chain_, logger_)
	    ->start();
}

} // namespace warp::server
