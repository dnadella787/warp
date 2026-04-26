#pragma once

#include <memory>
#include <boost/beast/core.hpp>

#include "coroutine_listener.hpp"
#include "callback_listener.hpp"
#include "warp/http/event_loop_mode.hpp"

namespace warp::http {

/*
 * compile time mapping of user facing event loop mode into
 * internal listener object types. i.e. coroutine event loop means coroutine listener
 * and coroutine http session as a result.
 */
template <event_loop_mode Mode>
struct listener_traits {
	using type = std::conditional_t<Mode == event_loop_mode::coroutines, coroutine_listener, callback_listener>;
};

} // namespace warp::http
