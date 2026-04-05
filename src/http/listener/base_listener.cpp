//
// Created by Dhanush Nadella on 4/5/26.
//

#include "base_listener.hpp"
#include <boost/asio/strand.hpp>
#include "../../common/util/fail.h"

namespace warp::http {

base_listener::base_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
                             const unsigned short port)
    : ioc_(ioc), acceptor_(boost::asio::make_strand(ioc)), registry_(registry) {
	auto const addr = boost::asio::ip::make_address(address);
	auto const endpoint = boost::asio::ip::tcp::endpoint {addr, port};
	boost::beast::error_code ec;

	acceptor_.open(endpoint.protocol(), ec);
	if (ec) {
		util::fail_except(ec, "base_runner", "open");
	}

	acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec) {
		util::fail_except(ec, "base_runner", "set_option{reuse_address=true}");
	}

	acceptor_.bind(endpoint, ec);
	if (ec) {
		util::fail_except(ec, "base_runner", "bind");
	}

	acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
	if (ec) {
		util::fail_except(ec, "base_runner", "listen");
	}
}

} // namespace warp::http
