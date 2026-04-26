#pragma once

namespace warp::server {

class base_listener {
public:
	virtual ~base_listener() = default;
	virtual void run() = 0;
};

} // namespace warp::server
