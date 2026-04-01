#pragma once

namespace warp::http {

enum class event_loop_mode {
	callbacks,
	coroutines
};

} // namespace warp::http