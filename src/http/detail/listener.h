# pragma once
#include <boost/beast/core.hpp>

#include "../../net/router/registry.hpp"

namespace warp::http::detail {

class listener : public std::enable_shared_from_this<listener> {
public:
    listener(boost::asio::io_context& ioc, net::router::registry &registry, const std::string& address, unsigned short port);

    void run();
private:
    void do_accept();
    void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    net::router::registry& registry_;

    static constexpr std::string COMPONENT{"listener"};
};

}

