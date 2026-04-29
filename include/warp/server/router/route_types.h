//
// Created by Dhanush Nadella on 4/29/26.
//

#pragma once
#include <concepts>

#include "warp/warp.hpp"
#include "warp/http/request.hpp"
#include "warp/http/response.hpp"

namespace warp::server {

class server_builder;
/**
 * @brief Trait to identify synchronous route handlers.
 * * Validates that the handler 'H' (after type decay) can be invoked with a
 * 'http::request' and returns a type convertible to 'http::response'.
 *
 * For both event loop modes, the handler is launched inline within the read loop
 * effectively blocking the next read until this handler completes. Choose wisely :P
 *
 * Notice that we allow lvalue by value, lvalue by ref, and const lvalue ref for sync
 * handlers because the sync handler is only borrowing the request from the event loop
 * on_read callback, it is not taking over the lifetime of the request like async_handler
 * does which is why we std::move for async_handler always but conditionally here to reduce
 * heap allocs.
 *
 * &fn because we want the test on lvalue callable object, not rvalue temp
 */
template <typename H>
inline constexpr bool is_movable_sync_route_handler = requires(std::decay_t<H> &fn, request req) {
	{ std::invoke(fn, std::move(req)) } -> std::convertible_to<response>;
};

template <typename H>
inline constexpr bool is_lvalue_sync_route_handler = requires(std::decay_t<H> &fn, request &req) {
	{ std::invoke(fn, req) } -> std::convertible_to<response>;
};

template <typename H>
inline constexpr bool is_sync_route_handler = is_movable_sync_route_handler<H> || is_lvalue_sync_route_handler<H>;

/**
 * @brief Trait to identify asynchronous route handlers. It requires that the
 * decayed handler function can be invoked using a warp::request to return
 * warp::awaitable<response>
 *
 * In both event loop modes, the async handler is launched as a separate coroutine
 * and does not block the read loop from reading in the next request in the http
 * session
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

/**
 * resource class is registerable with the server if the resource class has a
 * way to register itself with the builder. The class must be an L value.
 */
template <typename T>
concept resource_registrable = std::is_lvalue_reference_v<T> &&
                               requires(T resource, server_builder &builder) { resource.register_routes(builder); };
	
}
