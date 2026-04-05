//
// Created by Dhanush Nadella on 3/26/26.
//

#include "listener.hpp"

#include <boost/asio/strand.hpp>

#include "../session/http_session.hpp"
#include "../../common/util/fail.h"

namespace warp::http {

listener::listener(boost::asio::io_context &ioc, registry &registry, const std::string &address, unsigned short port)
    : base_listener(ioc, registry, address, port) {
}

void listener::run() {
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session.
	boost::asio::dispatch(this->acceptor_.get_executor(),
	                      boost::beast::bind_front_handler(&listener::do_accept, this->shared_from_this()));
}

void listener::do_accept() {
	// The new connection gets its own strand
	acceptor_.async_accept(boost::asio::make_strand(ioc_),
	                       boost::beast::bind_front_handler(&listener::on_accept, shared_from_this()));
}

void listener::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
	if (ec)
		util::fail(ec, COMPONENT, "on_accept");
	else
		std::make_shared<http_session>(std::move(socket), registry_)->start();

	do_accept();
}
} // namespace warp::http
