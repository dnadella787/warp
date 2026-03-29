#include "warp/http/server.hpp"

#include <algorithm>
#include <memory>

#include "server_impl.hpp"
#include "registry.hpp"

namespace warp::http {

server_builder &server_builder::address(std::string address) {
	address_ = std::move(address);
	return *this;
}

server_builder &server_builder::port(std::uint16_t port) {
	port_ = port;
	return *this;
}

server_builder &server_builder::worker_threads(std::size_t count) {
	workers_ = std::max<std::size_t>(1, count);
	return *this;
}

server_builder &server_builder::route_async(method verb, std::string path, async_handler handler) {
	routes_.push_back(route_definition {.verb = verb, .path = std::move(path), .callback = std::move(handler)});
	return *this;
}

server server_builder::build() const {
	registry registry;
	for (const auto &route : routes_) {
		registry.add(route.verb, route.path, route.callback);
	}
	auto impl = std::make_shared<server::impl>(address_, port_, workers_, std::move(registry));
	return server {std::move(impl)};
}

server::server() = default;
server::~server() {
	stop();
}

server::server(std::shared_ptr<impl> impl) : impl_(std::move(impl)) {
}
server::server(server &&) noexcept = default;
server &server::operator=(server &&) noexcept = default;

void server::run(bool blocking) {
	if (impl_) {
		impl_->run(blocking);
	}
}

void server::stop() {
	if (impl_) {
		impl_->stop();
	}
}

server::controller server::get_controller() const {
	return controller {impl_};
}

server::controller::controller(const std::shared_ptr<impl> &impl) : impl_(impl) {
}

void server::controller::stop() {
	if (auto locked = impl_.lock()) {
		locked->stop();
	}
}

} // namespace warp::http
