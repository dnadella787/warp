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

server_builder &server_builder::ssl_config(warp::ssl::ssl_config config) {
	ssl_config_ = std::move(config);
	return *this;
}

server_builder &server_builder::add_job(job::background_job job) {
	jobs_.push_back(std::move(job));
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

template <http::event_loop_mode Mode>
[[nodiscard]] std::shared_ptr<server::impl_base> server_builder::make_impl() const {
	registry registry;
	std::vector<http::handler> route_handlers(routes_.size());
	for (const auto &[verb, path, constraints, handler] : routes_) {
		const auto route_id = registry.add_typed(verb, path, constraints);
		route_handlers[route_id.index()] = handler;
	}

	auto req_chain_entries = build_interceptor_chain_entries(req_interceptors_);
	auto resp_chain_entries = build_interceptor_chain_entries(resp_interceptors_);

	return std::make_shared<server::server_impl<Mode>>(
	    address_, port_, workers_, std::move(registry), std::move(route_handlers), ssl_config_, jobs_,
	    interceptor_chain<request> {std::move(req_chain_entries)},
	    interceptor_chain<response> {std::move(resp_chain_entries)}, logger_.value_or(log::default_logger()));
}

template <detail::erased_interceptor_type Interceptor>
std::vector<Interceptor>
server_builder::build_interceptor_chain_entries(std::vector<interceptor_definition<Interceptor>> interceptors) {
	// we use stable sort for sorting the request/response interceptors so that we can preserve
	// the order in which they were registered for interceptors with the same priority
	std::stable_sort(interceptors.begin(), interceptors.end(),
	                 [](const auto &lhs, const auto &rhs) { return lhs.priority < rhs.priority; });

	std::vector<Interceptor> chain_entries;
	chain_entries.reserve(interceptors.size());

	// convert from interceptor_def to Interceptor object itself for the chain
	std::ranges::transform(interceptors, std::back_inserter(chain_entries),
	                       [](auto &entry) { return std::move(entry.callback); });

	return chain_entries;
}

// tell the compiler to instantiate these concrete specializations, otherwise we would need the entire
// templated impl in .hpp file in include dir which we want to hide from user
template server server_builder::build<http::event_loop_mode::callbacks>() const;
template server server_builder::build<http::event_loop_mode::coroutines>() const;

} // namespace warp::server
