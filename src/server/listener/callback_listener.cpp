//
// Created by Dhanush Nadella on 3/26/26.
//

#include "callback_listener.hpp"

#include <boost/asio/strand.hpp>

#include "server/session/callback_http_session.hpp"

namespace warp::server {

template <warp_session_transport Transport>
callback_listener<Transport>::callback_listener(boost::asio::io_context &ioc,
                                                transport_provider<Transport> transport_provider,
                                                const registry &registry,
                                                const std::vector<http::handler> &route_handlers,
                                                const interceptor_chain<request> &req_interceptor_chain,
                                                const interceptor_chain<response> &resp_interceptor_chain,
                                                const std::string &address, unsigned short port, log::logger logger)
    : base_type(ioc, std::move(transport_provider), registry, route_handlers, req_interceptor_chain,
                resp_interceptor_chain, address, port, std::move(logger)) {
}

template <warp_session_transport Transport>
void callback_listener<Transport>::run() {
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session.
	boost::asio::dispatch(this->acceptor_.get_executor(),
	                      boost::beast::bind_front_handler(&callback_listener::do_accept, this->shared_from_this()));
}

template <warp_session_transport Transport>
void callback_listener<Transport>::do_accept() {
	// The new connection gets its own strand
	this->acceptor_.async_accept(
	    boost::asio::make_strand(this->ioc_),
	    boost::beast::bind_front_handler(&callback_listener::on_accept, this->shared_from_this()));
}

template <warp_session_transport Transport>
void callback_listener<Transport>::on_accept(beast::error_code ec, boost::asio::ip::tcp::socket socket) {
	if (ec) {
		// only happens if something server side shut the tcp acceptor so this indicates the parent server_impl
		// was shutdown
		// TODO: maybe also check parent server_impl status before just returning
		if (ec == boost::asio::error::operation_aborted)
			return;

		this->logger_.error("error in listener during on_accept: {}", ec.message());
	} else {
		this->start_session(std::move(socket));
	}

	do_accept();
}

template class callback_listener<plain_session_transport>;
template class callback_listener<tls_session_transport>;

} // namespace warp::server
