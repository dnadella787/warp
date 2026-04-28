//
// Created by Dhanush Nadella on 4/4/26.
//

#include "warp/server/server_builder.hpp"
#include "server_impl.hpp"

#include "router/registry.hpp"

namespace warp::server {

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

template <http::event_loop_mode Mode>
server server_builder::build() const {
	return make_server<Mode>();
}

template <http::event_loop_mode Mode>
[[nodiscard]] server server_builder::make_server() const {
	return server {make_impl<Mode>()};
}

template server server_builder::build<http::event_loop_mode::callbacks>() const;
template server server_builder::build<http::event_loop_mode::coroutines>() const;

template <http::event_loop_mode Mode>
[[nodiscard]] std::shared_ptr<server::impl_base> server_builder::make_impl() const {
	registry registry;
	for (const auto &route : routes_)
		registry.add_typed(route.verb, route.path, route.typed_query_constraints, route.callback);

	return std::make_shared<server::server_impl<Mode>>(address_, port_, workers_, std::move(registry));
}

} // namespace warp::server
