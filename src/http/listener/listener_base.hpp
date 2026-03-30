#pragma once

namespace warp::http {

class listener_base {
public:
	virtual ~listener_base() = default;
	virtual void run() = 0;
};

} // namespace warp::http
