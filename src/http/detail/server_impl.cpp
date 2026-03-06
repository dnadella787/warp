#include "server_impl.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "session.hpp"
#include "warp/http/server.hpp"
#include "../../net/core/io_context_pool.hpp"

namespace warp::http {

server::impl::impl(std::string address,
                   std::uint16_t port,
                   std::size_t workers,
                   net::router::registry routes)
    : address_(std::move(address))
    , port_(port)
    , pool_(workers)
    , accept_ctx_(std::make_shared<boost::asio::io_context>())
    , routes_(routes) {
    boost::asio::ip::tcp::resolver resolver(*accept_ctx_);
    auto endpoints = resolver.resolve(address_, std::to_string(port_));
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(*accept_ctx_);
    boost::asio::ip::tcp::endpoint endpoint = *endpoints.begin();
    acceptor_->open(endpoint.protocol());
    acceptor_->set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen();
}

void server::impl::run() {
    if (running_.exchange(true)) {
        return;
    }
    do_accept();
    pool_.run();
    accept_ctx_->run();
    running_.store(false);
}

void server::impl::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    boost::asio::dispatch(*accept_ctx_, [acceptor = acceptor_.get()]() {
        if (!acceptor) {
            return;
        }
        boost::system::error_code ec;
        acceptor->cancel(ec);
        acceptor->close(ec);
    });
    accept_ctx_->stop();
    pool_.stop();
}

std::uint16_t server::impl::port() const {
    boost::system::error_code ec;
    auto ep = acceptor_ ? acceptor_->local_endpoint(ec) : boost::asio::ip::tcp::endpoint{};
    if (ec) {
        return 0;
    }
    return ep.port();
}

void server::impl::do_accept() {
    if (!running_) {
        return;
    }
    acceptor_->async_accept(
        boost::asio::make_strand(pool_.next()),
        [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<detail::session>(std::move(socket), self->routes_)->start();
            }
            self->do_accept();
        });
}

} // namespace warp::http
