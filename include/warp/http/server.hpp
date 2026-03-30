#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>

#include "warp/http/body_builder.hpp"
#include "warp/http/event_loop_mode.hpp"
#include "warp/http/request.hpp"
#include "warp/http/response.hpp"
#include "warp/http/response_builder.hpp"

namespace warp::http {

using headers = request::fields_type;
using method = boost::beast::http::verb;
template <typename T>
using awaitable = boost::asio::awaitable<T>;
using handler = std::function<response(const request &)>;
using async_handler = std::function<awaitable<response>(request &&)>;

namespace detail {

template <typename H>
inline constexpr bool is_sync_route_handler = std::is_invocable_r_v<response, std::decay_t<H> &, request>;

template <typename H>
inline constexpr bool is_async_route_handler =
    std::invocable<std::decay_t<H> &, request> &&
    std::same_as<std::remove_cvref_t<std::invoke_result_t<std::decay_t<H> &, request>>, awaitable<response>>;

template <typename H>
concept route_handler = is_sync_route_handler<H> || is_async_route_handler<H>;

} // namespace detail

class server;

class server_builder {
public:
	server_builder() = default;

	server_builder &address(std::string address);
	server_builder &port(std::uint16_t port);
	server_builder &worker_threads(std::size_t count);
	server_builder &event_loop(event_loop_mode mode);

	template <typename H>
	    requires detail::route_handler<H>
	server_builder &route(method verb, std::string path, H &&handler) {
		return route_async(verb, std::move(path), make_async_handler(std::forward<H>(handler)));
	}

	template <typename H>
	    requires detail::route_handler<H>
	server_builder &route(std::string path, H &&handler) {
		return route(method::get, std::move(path), std::forward<H>(handler));
	}

	template <typename H>
	    requires detail::route_handler<H>
	server_builder &get(std::string path, H &&handler) {
		return route(method::get, std::move(path), std::forward<H>(handler));
	}

	template <typename H>
	    requires detail::route_handler<H>
	server_builder &post(std::string path, H &&handler) {
		return route(method::post, std::move(path), std::forward<H>(handler));
	}

	template <typename H>
	    requires detail::route_handler<H>
	server_builder &put(std::string path, H &&handler) {
		return route(method::put, std::move(path), std::forward<H>(handler));
	}

	template <typename H>
	    requires detail::route_handler<H>
	server_builder &delete_(std::string path, H &&handler) {
		return route(method::delete_, std::move(path), std::forward<H>(handler));
	}

	[[nodiscard]] server build() const;

private:
	template <typename H>
	static async_handler make_async_handler(H &&handler) {
		using fn_type = std::decay_t<H>;
		auto fn = fn_type(std::forward<H>(handler));

		if constexpr (detail::is_sync_route_handler<fn_type>) {
			return [fn = std::move(fn)](request &&req) mutable -> awaitable<response> {
				co_return std::invoke(fn, std::move(req));
			};
		} else {
			static_assert(detail::is_async_route_handler<fn_type>,
			              "route handler must return warp::response or warp::awaitable<warp::response>");
			return [fn = std::move(fn)](request &&req) mutable -> awaitable<response> {
				co_return co_await std::invoke(fn, std::move(req));
			};
		}
	}

	server_builder &route_async(method verb, std::string path, async_handler handler);

	struct route_definition {
		method verb;
		std::string path;
		async_handler callback;
	};

	std::string address_ {"0.0.0.0"};
	std::uint16_t port_ {8080};
	std::size_t workers_ {std::max<std::size_t>(1, std::thread::hardware_concurrency())};
	event_loop_mode event_loop_mode_ {event_loop_mode::callbacks};
	std::vector<route_definition> routes_;
};

class server {
	class impl;

public:
	server();
	~server();
	server(server &&) noexcept;
	server &operator=(server &&) noexcept;

	void run(bool blocking = true);
	void stop();

	// note that controller keeps a reference to the impl and not the server
	// object itself since server is a stack allocated obj, we do not keep any shared_ptr
	// references to it anywhere. This way we can entirely decouple the lifetimes of the
	// server and controller objects
	class controller {
	public:
		void stop();

	private:
		friend class server;
		explicit controller(const std::shared_ptr<impl> &impl);
		std::weak_ptr<impl> impl_;
	};

	[[nodiscard]] controller get_controller() const;

private:
	friend class server_builder;
	explicit server(std::shared_ptr<impl> impl);

	// we use std::shared_ptr instead of std::unique_ptr so controller can get a std::weak_ptr to impl
	std::shared_ptr<impl> impl_;
};

} // namespace warp::http

namespace warp {

using body_builder = http::body_builder;
using response_builder = http::response_builder;
using request = http::request;
using response = http::response;
using headers = http::headers;
using method = http::method;
template <typename T>
using awaitable = http::awaitable<T>;
using handler = http::handler;
using async_handler = http::async_handler;

} // namespace warp
