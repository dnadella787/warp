//
// Created by Dhanush Nadella on 4/4/26.
//

#include "warp/server/server_builder.hpp"

#include <algorithm>

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

server_builder &server_builder::logger(log::logger logger) {
	logger_ = std::move(logger);
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

template server server_builder::build<event_loop_mode::callbacks>() const;
template server server_builder::build<event_loop_mode::coroutines>() const;

template <http::event_loop_mode Mode>
[[nodiscard]] std::shared_ptr<server::impl_base> server_builder::make_impl() const {
	registry registry;
	for (const auto &[verb, path, constraints, handler] : routes_)
		registry.add_typed(verb, path, constraints, handler);

	auto interceptors = interceptors_;
	std::stable_sort(interceptors.begin(), interceptors.end(),
	                 [](const auto &lhs, const auto &rhs) { return lhs.priority < rhs.priority; });

	std::vector<detail::type_erased_interceptor> chain_entries;
	chain_entries.reserve(interceptors.size());
	for (auto &entry : interceptors)
		chain_entries.push_back(std::move(entry.callback));

	return std::make_shared<server::server_impl<Mode>>(address_, port_, workers_, std::move(registry),
	                                                   interceptor_chain {std::move(chain_entries)},
	                                                   logger_.value_or(log::default_logger()));
}

} // namespace warp::server
