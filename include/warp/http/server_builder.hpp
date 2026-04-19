//
// Created by Dhanush Nadella on 4/4/26.
//

#pragma once
#include <string>

#include "http.hpp"
#include "event_loop_mode.hpp"
#include "route_constraints.hpp"
#include "server.hpp"

namespace warp::http {
/**
 * @brief Trait to identify synchronous route handlers.
 * * Validates that the handler 'H' (after type decay) can be invoked with a
 * 'request' and returns a type convertible to 'response'. This supports
 * standard, blocking request-handling logic.
 *
 * For both http_session and coroutine_http_session, the handler is
 * launched synchronously.
 */
template <typename H>
inline constexpr bool is_sync_route_handler = requires(std::decay_t<H> &fn, request req) {
	{ std::invoke(fn, std::move(req)) } -> std::convertible_to<response>;
};

/**
 * @brief Trait to identify asynchronous route handlers (C++20 Coroutines).
 * * Validates that the handler 'H' is invocable with a 'request' and returns
 * exactly an 'awaitable<response>'. This ensures compatibility with
 * non-blocking I/O patterns powered by Boost Asio.
 *
 * For http_session, the handler is launched as a coroutine but in
 * coroutine_http_session the handler is launched as a child coroutine
 */
template <typename H>
inline constexpr bool is_async_route_handler = requires(std::decay_t<H> &fn, request req) {
	{ std::invoke(fn, std::move(req)) } -> std::same_as<awaitable<response>>;
};

/**
 * @brief compile time requirement for request handlers
 */
template <typename H>
concept route_handler = is_async_route_handler<H> || is_sync_route_handler<H>;

template <typename T>
concept resource_registrable = std::is_lvalue_reference_v<T> &&
                               requires(T resource, server_builder &builder) { resource.register_routes(builder); };

class server_builder {
private:
	server_builder &route(method verb, std::string path, handler handler);

public:
	server_builder() = default;

	server_builder &address(std::string address);
	server_builder &port(std::uint16_t port);
	server_builder &worker_threads(std::size_t count);
	server_builder &event_loop(event_loop_mode mode);

	template <resource_registrable Resource>
	server_builder &register_resource(Resource &&resource) {
		std::forward<Resource>(resource).register_routes(*this);
		return *this;
	}

	template <typename Resource>
	    requires(!std::is_lvalue_reference_v<Resource &&>)
	server_builder &register_resource(Resource &&) = delete;

	template <route_handler H>
	server_builder &route(method verb, std::string path, H &&handler) {
		return route_runtime(verb, std::move(path), make_route_handler(std::forward<H>(handler)));
	}

	template <fixed_string Path, route_handler H>
	server_builder &route(method verb, route_path<Path>, H &&handler) {
		return route(verb, std::string(route_path<Path>::view()), std::forward<H>(handler));
	}

	template <route_registration_spec Spec, route_handler H>
	server_builder &route(Spec, H &&handler) {
		return route(Spec::verb, registered_path<Spec>(), std::forward<H>(handler));
	}

	template <method Verb, fixed_string Path, query_constraint... QueryConstraints, route_handler H>
	server_builder &route(H &&handler) {
		return route(route_spec<Verb, Path, QueryConstraints...> {}, std::forward<H>(handler));
	}

	template <route_handler H>
	server_builder &route(std::string path, H &&handler) {
		return route(method::get, std::move(path), std::forward<H>(handler));
	}

	template <fixed_string Path, route_handler H>
	server_builder &route(route_path<Path>, H &&handler) {
		return route(method::get, route_path<Path> {}, std::forward<H>(handler));
	}

	template <route_handler H>
	server_builder &get(std::string path, H &&handler) {
		return route(method::get, std::move(path), std::forward<H>(handler));
	}

	template <fixed_string Path, route_handler H>
	server_builder &get(route_path<Path>, H &&handler) {
		return route(method::get, route_path<Path> {}, std::forward<H>(handler));
	}

	template <fixed_string Path, query_constraint... QueryConstraints, route_handler H>
	server_builder &get(H &&handler) {
		return route<method::get, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	template <route_handler H>
	server_builder &post(std::string path, H &&handler) {
		return route(method::post, std::move(path), std::forward<H>(handler));
	}

	template <fixed_string Path, route_handler H>
	server_builder &post(route_path<Path>, H &&handler) {
		return route(method::post, route_path<Path> {}, std::forward<H>(handler));
	}

	template <fixed_string Path, query_constraint... QueryConstraints, route_handler H>
	server_builder &post(H &&handler) {
		return route<method::post, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	template <route_handler H>
	server_builder &put(std::string path, H &&handler) {
		return route(method::put, std::move(path), std::forward<H>(handler));
	}

	template <fixed_string Path, route_handler H>
	server_builder &put(route_path<Path>, H &&handler) {
		return route(method::put, route_path<Path> {}, std::forward<H>(handler));
	}

	template <fixed_string Path, query_constraint... QueryConstraints, route_handler H>
	server_builder &put(H &&handler) {
		return route<method::put, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	template <route_handler H>
	server_builder &delete_(std::string path, H &&handler) {
		return route(method::delete_, std::move(path), std::forward<H>(handler));
	}

	template <fixed_string Path, route_handler H>
	server_builder &delete_(route_path<Path>, H &&handler) {
		return route(method::delete_, route_path<Path> {}, std::forward<H>(handler));
	}

	template <fixed_string Path, query_constraint... QueryConstraints, route_handler H>
	server_builder &delete_(H &&handler) {
		return route<method::delete_, Path, QueryConstraints...>(std::forward<H>(handler));
	}

	[[nodiscard]] server build() const;

private:
	server_builder &route_runtime(method verb, std::string path, handler callback) {
		return route(verb, std::move(path), std::move(callback));
	}

	template <route_registration_spec Spec>
	[[nodiscard]] static std::string registered_path() {
		std::string path(Spec::path_view());
		bool first = true;
		for (const auto &constraint : Spec::query_constraints) {
			path.push_back(first ? '?' : '&');
			first = false;
			if (constraint.presence == query_constraint_presence::forbidden) {
				path.push_back('!');
			} else if (constraint.presence == query_constraint_presence::optional) {
				path.push_back('~');
			}
			path.append(constraint.name);
			if (constraint.has_exact_value) {
				path.push_back('=');
				path.append(constraint.exact_value);
			}
		}
		return path;
	}

	template <event_loop_mode Mode>
	[[nodiscard]] server make_server() const;

	// Upcasting the shared_ptr implicitly from server_impl and impl_base
	template <event_loop_mode Mode>
	[[nodiscard]] std::shared_ptr<server::impl_base> make_impl() const;

	template <route_handler H>
	static handler make_route_handler(H &&handler) {
		using fn_type = std::decay_t<H>;
		auto fn = fn_type(std::forward<H>(handler));

		// TODO: Add support for executing sync handlers as coroutines too
		// so that a long running sync handler does not block the event loop
		// from reading more requests. User should use async handler for these
		// but just in case...
		if constexpr (is_sync_route_handler<fn_type>) {
			return sync_handler {
			    [fn = std::move(fn)](request req) mutable -> response { return std::invoke(fn, std::move(req)); }};
		} else {
			return async_handler {[fn = std::move(fn)](request &&req) mutable -> awaitable<response> {
				co_return co_await std::invoke(fn, std::move(req));
			}};
		}
	}

	struct route_definition {
		method verb;
		std::string path;
		handler callback;
	};

	std::string address_ {"0.0.0.0"};
	std::uint16_t port_ {8080};
	std::size_t workers_ {1};
	event_loop_mode event_loop_mode_ {event_loop_mode::callbacks};
	std::vector<route_definition> routes_;
};

} // namespace warp::http
