# pragma once
#include <boost/beast/core.hpp>

namespace warp::http::detail {

class listener : public std::enable_shared_from_this<listener> {
public:
    listener::listener(boost::asio::io_context& ioc, const std::string& address, unsigned short port);

    void run();
private:
    static void fail(boost::beast::error_code &ec);
    void do_accept();
    void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

}

