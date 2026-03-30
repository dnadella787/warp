#pragma once

namespace warp::http {

enum class event_loop_mode {
	callbacks,
	coroutines
};

} // namespace warp::http

namespace warp {

using event_loop_mode = http::event_loop_mode;

} // namespace warp
