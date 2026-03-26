//
// Created by Dhanush Nadella on 3/26/26.
//

#include "listener.h"

#include <iostream>

#include <boost/asio/strand.hpp>

#include "session.hpp"

namespace warp::http::detail {

listener::listener(boost::asio::io_context &ioc,  net::router::registry &registry, const std::string& address, const unsigned short port)
    : ioc_(ioc), registry_(registry), acceptor_(boost::asio::make_strand(ioc)) {
	auto const addr = boost::asio::ip::make_address(address);
	auto const endpoint = boost::asio::ip::tcp::endpoint{addr, port};
	boost::beast::error_code ec;

	// Open the acceptor
	acceptor_.open(endpoint.protocol(), ec);
	if (ec) {
		fail(ec);
	}

	// Allow address reuse
	acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec) {
		fail(ec);
	}

	// Bind to the server address
	acceptor_.bind(endpoint, ec);
	if (ec) {
		fail(ec);
	}

	// Start listening for connections
	acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
	if (ec) {
		fail(ec);
	}
}

void listener::run() {
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session. Although not strictly necessary
	// for single-threaded contexts, this example code is written to be
	// thread-safe by default.
	boost::asio::dispatch(acceptor_.get_executor(), boost::beast::bind_front_handler(&listener::do_accept, this->shared_from_this()));
}

void listener::fail(boost::beast::error_code &ec) {
	std::cerr << ec.message() << std::endl;
	throw std::runtime_error("Error in TCP listener: " + ec.message());
}

void listener::do_accept() {
	// The new connection gets its own strand
	acceptor_.async_accept(net::make_strand(ioc_), boost::beast::bind_front_handler(&listener::on_accept, shared_from_this()));
}

void listener::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
{
	if(ec)
	{
		fail(ec);
	}
	else
	{
		// Create the http session and start it
		std::make_shared<session>(std::move(socket), registry_)->start();
	}

	// Accept another connection
	do_accept();
}

} // namespace warp::http::detail
