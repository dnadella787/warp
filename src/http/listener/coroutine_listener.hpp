#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core.hpp>

#include "base_listener.hpp"
#include "../router/registry.hpp"

namespace warp::http {

class coroutine_listener final : public base_listener, public std::enable_shared_from_this<coroutine_listener> {
public:
	coroutine_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
	                   unsigned short port);

	void run() override;

private:
	boost::asio::awaitable<void> accept_loop();

	static constexpr std::string_view COMPONENT {"coroutine_listener"};
};

} // namespace warp::http
