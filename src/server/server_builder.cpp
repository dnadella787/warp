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
	route_executor_table<Mode> route_executors(routes_.size());
	for (const auto &[verb, path, constraints, handler] : routes_) {
		const auto route_id = registry.add_typed(verb, path, constraints);
		route_executors.set(route_id, handler);
	}

	auto req_interceptors = req_interceptors_;
	std::stable_sort(req_interceptors.begin(), req_interceptors.end(),
	                 [](const auto &lhs, const auto &rhs) { return lhs.priority < rhs.priority; });

	std::vector<detail::type_erased_req_interceptor> req_chain_entries;
	req_chain_entries.reserve(req_interceptors.size());
	for (auto &entry : req_interceptors)
		req_chain_entries.push_back(std::move(entry.callback));

	auto resp_interceptors = resp_interceptors_;
	std::stable_sort(resp_interceptors.begin(), resp_interceptors.end(),
	                 [](const auto &lhs, const auto &rhs) { return lhs.priority < rhs.priority; });

	std::vector<detail::type_erased_resp_interceptor> resp_chain_entries;
	resp_chain_entries.reserve(resp_interceptors.size());
	for (auto &entry : resp_interceptors)
		resp_chain_entries.push_back(std::move(entry.callback));

	return std::make_shared<server::server_impl<Mode>>(
	    address_, port_, workers_, std::move(registry), std::move(route_executors),
	    interceptor_chain<request> {std::move(req_chain_entries)},
	    interceptor_chain<response> {std::move(resp_chain_entries)}, logger_.value_or(log::default_logger()));
}

} // namespace warp::server
