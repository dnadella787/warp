#pragma once

#include "server/router/route_runtime.hpp"
#include "server/session/policy/transport.h"
#include "warp/http/event_loop_mode.hpp"

namespace warp::server {

template <warp_session_transport Transport>
class callback_http_session;

template <warp_session_transport Transport>
class coroutine_http_session;

template <warp_session_transport Transport>
class callback_listener;

template <warp_session_transport Transport>
class coroutine_listener;

template <http::event_loop_mode Mode, warp_session_transport Transport>
struct event_loop_traits;

template <warp_session_transport Transport>
struct event_loop_traits<event_loop_mode::callbacks, Transport> {
	using listener_type = callback_listener<Transport>;
	using session_type = callback_http_session<Transport>;
	using route_runtime_type = route_runtime<session_type>;
};

template <warp_session_transport Transport>
struct event_loop_traits<event_loop_mode::coroutines, Transport> {
	using listener_type = coroutine_listener<Transport>;
	using session_type = coroutine_http_session<Transport>;
	using route_runtime_type = route_runtime<session_type>;
};

} // namespace warp::server
