//
// Created by Dhanush Nadella on 4/4/26.
//

#include "warp/http/server_builder.hpp"
#include "server_impl.hpp"

#include "router/registry.hpp"

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

server_builder &server_builder::event_loop(event_loop_mode mode) {
	event_loop_mode_ = mode;
	return *this;
}

server_builder &server_builder::route_async(method verb, std::string path, async_handler handler) {
	routes_.push_back(route_definition {.verb = verb, .path = std::move(path), .callback = std::move(handler)});
	return *this;
}

server server_builder::build() const {
	switch (event_loop_mode_) {
	case event_loop_mode::callbacks:
		return make_server<event_loop_mode::callbacks>();
	case event_loop_mode::coroutines:
		return make_server<event_loop_mode::coroutines>();
	default:
		throw std::runtime_error("Invalid event loop mode");
	}
}

template <event_loop_mode Mode>
[[nodiscard]] server server_builder::make_server() const {
	return server {make_impl<Mode>()};
}

template <event_loop_mode Mode>
[[nodiscard]] std::shared_ptr<server::impl_base> server_builder::make_impl() const {
	registry registry;
	for (const auto &route : routes_) {
		registry.add(route.verb, route.path, route.callback);
	}
	return std::make_shared<server::server_impl<Mode>>(address_, port_, workers_, std::move(registry));
}

} // namespace warp::http
