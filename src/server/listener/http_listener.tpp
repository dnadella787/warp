//
// Created by Dhanush Nadella on 4/5/26.
//
#include "http_listener.h"

#include <boost/asio/strand.hpp>

namespace warp::server {

template <typename T, typename RouteExecutors>
http_listener<T, RouteExecutors>::http_listener(boost::asio::io_context &ioc, const registry &registry,
                                                const RouteExecutors &route_executors,
                                                const interceptor_chain<request> &req_interceptor_chain,
                                                const interceptor_chain<response> &resp_interceptor_chain,
                                                const std::string &address, const unsigned short port,
                                                log::logger logger)
    requires warp_listener<T, RouteExecutors>
    : ioc_(ioc), acceptor_(boost::asio::make_strand(ioc)), registry_(registry), route_executors_(route_executors),
      req_interceptor_chain_(req_interceptor_chain), resp_interceptor_chain_(resp_interceptor_chain),
      logger_(std::move(logger)) {
    auto const addr = boost::asio::ip::make_address(address);
    auto const endpoint = boost::asio::ip::tcp::endpoint{addr, port};
    boost::beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        logger_.error("Error in base_listener during open: {}", ec.message());
        throw std::runtime_error(ec.message());
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        logger_.error("Error in base_listener during set_option{{reuse_address=true}}: {}", ec.message());
        throw std::runtime_error(ec.message());
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
        logger_.error("Error in base_listener during bind: {}", ec.message());
        throw std::runtime_error(ec.message());
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        logger_.error("Error in base_listener during listen: {}", ec.message());
        throw std::runtime_error(ec.message());
    }
}

template <typename T, typename RouteExecutors>
void http_listener<T, RouteExecutors>::run() {
    static_cast<T*>(this)->execute();
}
} // namespace warp::server
