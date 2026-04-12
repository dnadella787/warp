//
// Created by Dhanush Nadella on 4/5/26.
//
#include "http_listener.h"
#include "../../common/util/fail.h"
#include "boost/asio/strand.hpp"


namespace warp::http {

template <typename T>
http_listener<T>::http_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
                                  const unsigned short port) requires WarpListener<T>
    : ioc_(ioc), acceptor_(boost::asio::make_strand(ioc)), registry_(registry) {
    auto const addr = boost::asio::ip::make_address(address);
    auto const endpoint = boost::asio::ip::tcp::endpoint{addr, port};
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

template <typename  T>
void http_listener<T>::run() {
    static_cast<T*>(this)->execute();
}
}