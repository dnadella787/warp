//
// Created by Dhanush Nadella on 3/26/26.
//

#include "callback_listener.hpp"

#include <boost/asio/strand.hpp>

#include "server/session/callback_http_session.hpp"

namespace warp::server {

callback_listener::callback_listener(boost::asio::io_context &ioc, const registry &registry,
                                     const route_executor_table<http::event_loop_mode::callbacks> &route_executors,
                                     const interceptor_chain<request> &req_interceptor_chain,
                                     const interceptor_chain<response> &resp_interceptor_chain,
                                     const std::string &address, unsigned short port, log::logger logger)
    : http_listener(ioc, registry, route_executors, req_interceptor_chain, resp_interceptor_chain, address, port,
                    std::move(logger)) {
}

void callback_listener::execute() {
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session.
	boost::asio::dispatch(this->acceptor_.get_executor(),
	                      boost::beast::bind_front_handler(&callback_listener::do_accept, shared_from_this()));
}

void callback_listener::do_accept() {
	// The new connection gets its own strand
	acceptor_.async_accept(boost::asio::make_strand(ioc_),
	                       boost::beast::bind_front_handler(&callback_listener::on_accept, shared_from_this()));
}

void callback_listener::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
	if (ec) {
		// only happens if something server side shut the tcp acceptor so this indicates the parent server_impl
		// was shutdown
		// TODO: maybe also check parent server_impl status before just returning
		if (ec == boost::asio::error::operation_aborted)
			return;

		logger_.error("Error in listener during on_accept: {}", ec.message());
	} else {
		std::make_shared<callback_http_session>(std::move(socket), registry_, route_executors_, req_interceptor_chain_,
		                                        resp_interceptor_chain_, logger_)
		    ->start();
	}

	do_accept();
}
} // namespace warp::server
