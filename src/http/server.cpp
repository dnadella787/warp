#include "warp/http/server.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "warp/net/core/io_context_pool.hpp"
#include "warp/net/router.hpp"

namespace warp::http {

namespace {
using beast_request = boost::beast::http::request<boost::beast::http::string_body>;
using beast_response = boost::beast::http::response<boost::beast::http::string_body>;

warp::net::http::method map_method(boost::beast::http::verb v) {
    using verb = boost::beast::http::verb;
    switch (v) {
    case verb::get: return warp::net::http::method::get;
    case verb::post: return warp::net::http::method::post;
    case verb::put: return warp::net::http::method::put;
    case verb::delete_: return warp::net::http::method::delete_;
    case verb::head: return warp::net::http::method::head;
    case verb::options: return warp::net::http::method::options;
    case verb::patch: return warp::net::http::method::patch;
    default: return warp::net::http::method::unknown;
    }
}

warp::net::http::request to_request(const beast_request& req) {
    warp::net::http::headers hdrs;
    for (const auto& field : req.base()) {
        hdrs.emplace(field.name_string(), field.value());
    }
    return warp::net::http::request(map_method(req.method()), std::string(req.target()), req.body(), std::move(hdrs));
}

std::shared_ptr<beast_response> to_beast_response(const warp::net::http::response& resp, const beast_request& req) {
    auto be_resp = std::make_shared<beast_response>();
    be_resp->version(req.version());
    be_resp->result(resp.status());
    be_resp->body() = std::string(resp.body());
    for (const auto& [key, value] : resp.header_map()) {
        auto field = boost::beast::http::string_to_field(key);
        if (field == boost::beast::http::field::unknown) {
            be_resp->set(key, value);
        } else {
            be_resp->set(field, value);
        }
    }
    be_resp->prepare_payload();
    if (!be_resp->has_content_length()) {
        be_resp->content_length(be_resp->body().size());
    }
    be_resp->keep_alive(false);
    return be_resp;
}
} // namespace

class server::impl : public std::enable_shared_from_this<server::impl> {
public:
    impl(std::string address,
         std::uint16_t port,
         std::size_t workers,
         warp::net::router::registry routes);

    void run();
    void stop();

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
        beast_request request_;
    };

    std::string address_;
    std::uint16_t port_;
    warp::net::core::io_context_pool pool_;
    std::shared_ptr<boost::asio::io_context> accept_ctx_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    warp::net::router::registry routes_;
    std::atomic<bool> running_{false};
};

server_builder& server_builder::address(std::string address) {
    address_ = std::move(address);
    return *this;
}

server_builder& server_builder::port(std::uint16_t port) {
    port_ = port;
    return *this;
}

server_builder& server_builder::worker_threads(std::size_t count) {
    workers_ = std::max<std::size_t>(1, count);
    return *this;
}

server_builder& server_builder::route(std::string path, handler handler) {
    routes_.add(std::move(path), std::move(handler));
    return *this;
}

server server_builder::build() const {
    auto impl = std::make_shared<server::impl>(address_, port_, workers_, routes_);
    return server{std::move(impl)};
}

server::server() = default;
server::~server() {
    stop();
}

server::server(std::shared_ptr<impl> impl) : impl_(std::move(impl)) {}
server::server(server&&) noexcept = default;
server& server::operator=(server&&) noexcept = default;

void server::run() {
    if (impl_) {
        impl_->run();
    }
}

void server::stop() {
    if (impl_) {
        impl_->stop();
    }
}

server::control::control(std::shared_ptr<impl> impl) : impl_(std::move(impl)) {}

void server::control::stop() {
    if (auto locked = impl_.lock()) {
        locked->stop();
    }
}

server::control server::controller() const {
    return control{impl_};
}

server::impl::impl(std::string address,
                   std::uint16_t port,
                   std::size_t workers,
                   warp::net::router::registry routes)
    : address_(std::move(address))
    , port_(port)
    , pool_(workers)
    , accept_ctx_(std::make_shared<boost::asio::io_context>())
    , routes_(std::move(routes)) {
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

void server::impl::do_accept() {
    if (!running_) {
        return;
    }
    acceptor_->async_accept(
        boost::asio::make_strand(pool_.next()),
        [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<impl::session>(std::move(socket), self->routes_)->start();
            }
            self->do_accept();
        });
}

server::impl::session::session(boost::asio::ip::tcp::socket socket, warp::net::router::registry& routes)
    : stream_(std::move(socket))
    , routes_(routes) {}

void server::impl::session::start() {
    read();
}

void server::impl::session::read() {
    request_ = {};
    boost::beast::http::async_read(stream_, buffer_, request_,
        [self = shared_from_this()](boost::beast::error_code ec, std::size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        });
}

void server::impl::session::on_read(boost::beast::error_code ec, std::size_t) {
    if (ec == boost::beast::http::error::end_of_stream) {
        shutdown();
        return;
    }
    if (ec) {
        return;
    }

    auto warp_request = to_request(request_);
    warp::net::http::response resp;
    if (auto handler = routes_.find(warp_request.target())) {
        resp = (*handler)(warp_request);
    } else {
        resp = warp::net::http::response::not_found();
    }

    write_response(std::move(resp));
}

void server::impl::session::write_response(warp::net::http::response resp) {
    auto be_resp = to_beast_response(resp, request_);
    const bool close = !be_resp->keep_alive();
    boost::beast::http::async_write(stream_, *be_resp,
        [self = shared_from_this(), be_resp, close](boost::beast::error_code ec, std::size_t bytes_transferred) {
            self->on_write(ec, bytes_transferred, close);
        });
}

void server::impl::session::on_write(boost::beast::error_code ec, std::size_t, bool close) {
    if (ec) {
        return;
    }
    if (close) {
        shutdown();
        return;
    }
    read();
}

void server::impl::session::shutdown() {
    boost::system::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}

} // namespace warp::http
