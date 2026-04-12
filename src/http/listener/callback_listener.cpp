//
// Created by Dhanush Nadella on 3/26/26.
//

#include "callback_listener.hpp"

#include <boost/asio/strand.hpp>

#include "../session/callback_http_session.hpp"
#include "../../common/util/fail.h"

namespace warp::http {

callback_listener::callback_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
                                     unsigned short port)
    : http_listener(ioc, registry, address, port) {
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
	if (ec)
		util::fail(ec, COMPONENT, "on_accept");
	else
		std::make_shared<callback_http_session>(std::move(socket), registry_)->start();

	do_accept();
}
} // namespace warp::http
