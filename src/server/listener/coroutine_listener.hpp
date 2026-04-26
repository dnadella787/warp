#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core.hpp>

#include "http_listener.h"
#include "../router/registry.hpp"

namespace warp::server {

class coroutine_listener final : public http_listener<coroutine_listener>,
                                 public std::enable_shared_from_this<coroutine_listener> {
public:
	coroutine_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
	                   unsigned short port);

	void execute();

private:
	boost::asio::awaitable<void> accept_loop();

	static constexpr std::string_view COMPONENT {"coroutine_listener"};
};

} // namespace warp::server
