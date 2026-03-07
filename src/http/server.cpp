#include "warp/http/server.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "detail/server_impl.hpp"
#include "../net/router/registry.hpp"

namespace warp::http {

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
    routes_.emplace_back(std::move(path), std::move(handler));
    return *this;
}

server server_builder::build() const {
    net::router::registry registry;
    for (const auto& [path, handler] : routes_) {
        registry.add(path, handler);
    }
    auto impl = std::make_shared<server::impl>(address_, port_, workers_, std::move(registry));
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

server::controller server::get_controller() const {
    return controller{impl_};
}

server::controller::controller(std::shared_ptr<impl> impl) : impl_(std::move(impl)) {}

void server::controller::stop() {
    if (auto locked = impl_.lock()) {
        locked->stop();
    }
}

std::uint16_t server::port() const {
    if (!impl_) {
        return 0;
    }
    return impl_->port();
}

} // namespace warp::http
