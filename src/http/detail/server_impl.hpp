#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "warp/http/server.hpp"
#include "warp/net/core/io_context_pool.hpp"
#include "warp/net/router.hpp"

namespace warp::http {

class server::impl : public std::enable_shared_from_this<server::impl> {
public:
    impl(std::string address,
         std::uint16_t port,
         std::size_t workers,
         warp::net::router::registry routes);

    void run();
    void stop();
    [[nodiscard]] std::uint16_t port() const;

private:
    void do_accept();

    class session : public std::enable_shared_from_this<session> {
    public:
        session(boost::asio::ip::tcp::socket socket, warp::net::router::registry& routes);
        void start();

    private:
        void read();
        void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
        void write_response(warp::net::http::response resp);
        void on_write(boost::beast::error_code ec, std::size_t bytes_transferred, bool close);
        void shutdown();

        boost::beast::tcp_stream stream_;
        boost::beast::flat_buffer buffer_;
        warp::net::router::registry& routes_;
        boost::beast::http::request<boost::beast::http::string_body> request_;
    };

    std::string address_;
    std::uint16_t port_;
    warp::net::core::io_context_pool pool_;
    std::shared_ptr<boost::asio::io_context> accept_ctx_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    warp::net::router::registry routes_;
    std::atomic<bool> running_{false};
};

} // namespace warp::http
