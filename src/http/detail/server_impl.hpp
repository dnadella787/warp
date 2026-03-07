#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "warp/http/server.hpp"
#include "../../net/core/io_context_pool.hpp"
#include "../../net/router/registry.hpp"

namespace warp::http {

class server::impl : public std::enable_shared_from_this<impl> {
public:
    impl(
	std::string address, std::uint16_t port, std::size_t workers, net::router::registry routes);

    void run();
    void stop();
    [[nodiscard]] std::uint16_t port() const;

private:
    void do_accept();

    std::string address_;
    std::uint16_t port_;
    net::core::io_context_pool pool_;
    std::shared_ptr<boost::asio::io_context> accept_ctx_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    net::router::registry routes_;
    std::atomic<bool> running_{false};
};

} // namespace warp::http
