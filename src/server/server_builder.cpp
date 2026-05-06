//
// Created by Dhanush Nadella on 4/4/26.
//

#include "warp/server/server_builder.hpp"

#include <algorithm>

#include "server_impl.hpp"

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
	return std::make_shared<server::server_impl<Mode>>(address_, port_, workers_, routes_, ssl_config_, jobs_,
	                                                   req_interceptors_, resp_interceptors_,
	                                                   logger_.value_or(log::default_logger()));
}

// tell the compiler to instantiate these concrete specializations, otherwise we would need the entire
// templated impl in .hpp file in include dir which we want to hide from user
template server server_builder::build<http::event_loop_mode::callbacks>() const;
template server server_builder::build<http::event_loop_mode::coroutines>() const;

} // namespace warp::server
