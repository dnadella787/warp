//
// Created by Dhanush Nadella on 4/4/26.
//

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "interceptor/interceptor.h"
#include "job.hpp"
#include "router/route_types.h"
#include "warp/http/event_loop_mode.hpp"
#include "warp/http/http.hpp"
#include "warp/logging/logger.hpp"
#include "warp/server/router/route_spec.hpp"
#include "warp/server/server.hpp"
#include "warp/ssl/ssl_config.hpp"

namespace warp::server {

class server_builder {
public:
	server_builder() = default;

	server_builder &address(std::string address);
	server_builder &port(std::uint16_t port);
	server_builder &worker_threads(std::size_t count);
	server_builder &logger(log::logger logger);
	server_builder &ssl_config(warp::ssl::ssl_config config);
	server_builder &add_job(job::background_job job);

	template <job::job Job>
	server_builder &add_job(Job job) {
		return add_job(job::make_background_job(std::move(job)));
	}

	/*
	 * `register_resource` accepts lvalue
	 *
	 * Foo f; // lvalue
	 * server_builder().register_resource(f);
	 *
	 * template deduction for forwarding ref gives `Resource = Foo&`
	 * so the parameter type `Resource&&` collapses to `Foo&`
	 *
	 * Foo& &&foo -> Foo &foo from reference collapse (T& && -> T&, T& & -> T&)
	 *
	 * we don't have to worry about rvalue lifetime issues
	 * because the resource_registrable concept requires lvalue
	 */
	template <resource_registrable Resource>
	server_builder &register_resource(Resource &&resource) {
		std::forward<Resource>(resource).register_routes(*this);
		return *this;
	}

	template <http::fixed_string Path, http::query_constraint... QueryConstraints, route_handler H>
	server_builder &get(H &&handler) {
		return route<http::method::get, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	template <http::fixed_string Path, http::query_constraint... QueryConstraints, route_handler H>
	server_builder &post(H &&handler) {
		return route<http::method::post, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	template <http::fixed_string Path, http::query_constraint... QueryConstraints, route_handler H>
	server_builder &put(H &&handler) {
		return route<http::method::put, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	template <http::fixed_string Path, http::query_constraint... QueryConstraints, route_handler H>
	server_builder &delete_(H &&handler) {
		return route<http::method::delete_, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	// TODO: this one exists for the http_adapter codegen compilation, probably use friend + free function
	// to not make this public in general
	template <http::route_registration_spec Spec, route_handler H>
	server_builder &route(H &&handler) {
		return route_typed(Spec::verb, std::string(Spec::path_view()),
		                   std::vector<http::query_constraint_descriptor>(Spec::query_constraints.begin(),
		                                                                  Spec::query_constraints.end()),
		                   make_route_handler(std::forward<H>(handler)));
	}

	template <int Priority, http::request_interceptor Interceptor>
	server_builder &interceptor(Interceptor &&interceptor_obj) {
		req_interceptors_.push_back(interceptor_definition<detail::type_erased_req_interceptor> {
		    .priority = Priority,
		    .registration_order = next_interceptor_registration_order_++,
		    .callback = make_interceptor(std::forward<Interceptor>(interceptor_obj))});
		return *this;
	}

	template <int Priority, http::response_interceptor Interceptor>
	server_builder &interceptor(Interceptor &&interceptor_obj) {
		resp_interceptors_.push_back(interceptor_definition<detail::type_erased_resp_interceptor> {
		    .priority = Priority,
		    .registration_order = next_interceptor_registration_order_++,
		    .callback = make_response_interceptor(std::forward<Interceptor>(interceptor_obj))});
		return *this;
	}

	// default mode is callback
	template <http::event_loop_mode Mode = event_loop_mode::callbacks>
	[[nodiscard]] server build() const;

private:
	template <http::method Verb, http::fixed_string Path, http::query_constraint... QueryConstraints, route_handler H>
	server_builder &route(H &&handler) {
		// this is where the compile time validations happen and end
		// before it is passed off to runtime behavior, i.e. std::vector::push_back(...)
		return route<http::route_spec<Verb, Path, QueryConstraints...>>(std::forward<H>(handler));
	}

	server_builder &route_typed(http::method verb, std::string path,
	                            std::vector<http::query_constraint_descriptor> query_constraints,
	                            http::handler handler) {
		routes_.push_back(route_definition {.verb = verb,
		                                    .path = std::move(path),
		                                    .typed_query_constraints = std::move(query_constraints),
		                                    .callback = std::move(handler)});
		return *this;
	}

	template <http::event_loop_mode Mode>
	[[nodiscard]] server make_server() const;

	// Upcasting the shared_ptr implicitly from server_impl and impl_base
	template <http::event_loop_mode Mode>
	[[nodiscard]] std::shared_ptr<server::impl_base> make_impl() const;

	template <route_handler H>
	static http::handler make_route_handler(H &&handler) {
		using fn_type = std::decay_t<H>;
		auto fn = fn_type(std::forward<H>(handler));

		// TODO: Add support for executing sync handlers as coroutines too
		// so that a long running sync handler does not block the event loop
		// from reading more requests. User should use async handler for these
		// but just in case...

		// sync_handler for request& and const request& (rvalue can bind to const request&)
		if constexpr (is_lvalue_sync_route_handler<fn_type>) {
			return http::sync_handler {
			    [fn = std::move(fn)](http::request req) mutable -> http::response { return std::invoke(fn, req); }};
		}
		// sync_handler for request (by value) which needs std::move for zero allocations
		else if constexpr (is_movable_sync_route_handler<fn_type>) {
			return http::sync_handler {[fn = std::move(fn)](http::request req) mutable -> http::response {
				return std::invoke(fn, std::move(req));
			}};
		}
		// async handlers
		else {
			return http::async_handler {
			    [fn = std::move(fn)](http::request req) mutable -> http::awaitable<http::response> {
				    return std::invoke(fn, std::move(req));
			    }};
		}
	}

	template <http::request_interceptor Interceptor>
	static detail::type_erased_req_interceptor make_interceptor(Interceptor &&interceptor) {
		/**
		 * lambda caputres will deduce lvalue ref, rvalue by value (meaning copy)
		 * TODO: maybe use std::move or std::ref instead in the future to reduce alloc
		 * but also lifetime issues between server and interceptor obj
		 */
		return [interceptor = std::forward<Interceptor>(interceptor)](request &req) -> http::req_interceptor_result {
			using result_t = decltype(interceptor.intercept(req));
			if constexpr (std::is_void_v<result_t>) {
				interceptor.intercept(req);
				return std::nullopt;
			} else {
				return interceptor.intercept(req);
			}
		};
	}

	template <http::response_interceptor Interceptor>
	static detail::type_erased_resp_interceptor make_response_interceptor(Interceptor &&interceptor) {
		return [interceptor = std::forward<Interceptor>(interceptor)](response &resp) -> void {
			interceptor.intercept(resp);
		};
	}

	struct route_definition {
		http::method verb;
		std::string path;
		std::vector<http::query_constraint_descriptor> typed_query_constraints;
		http::handler callback;
	};

	/**
	 * @tparam TypeErasedInterceptorFunc can be of type type_erased_req_interceptor or type_erased_resp_interceptor
	 */
	template <detail::erased_interceptor_type TypeErasedInterceptorFunc>
	struct interceptor_definition {
		int priority;
		std::size_t registration_order;
		TypeErasedInterceptorFunc callback;
	};

	template <detail::erased_interceptor_type Interceptor>
	[[nodiscard]] static std::vector<Interceptor>
	build_interceptor_chain_entries(std::vector<interceptor_definition<Interceptor>> interceptors);

	std::string address_ {"0.0.0.0"};
	std::uint16_t port_ {8080};
	std::size_t workers_ {1};
	std::optional<log::logger> logger_;
	warp::ssl::ssl_config ssl_config_ {};
	std::vector<route_definition> routes_;
	std::vector<job::background_job> jobs_;
	std::vector<interceptor_definition<detail::type_erased_req_interceptor>> req_interceptors_;
	std::vector<interceptor_definition<detail::type_erased_resp_interceptor>> resp_interceptors_;
	std::size_t next_interceptor_registration_order_ {};
};

} // namespace warp::server
