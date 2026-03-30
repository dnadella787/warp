#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core.hpp>

#include "listener_base.hpp"
#include "../router/registry.hpp"

namespace warp::http {

class coroutine_listener final : public listener_base, public std::enable_shared_from_this<coroutine_listener> {
public:
	coroutine_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address, unsigned short port);

	void run() override;

private:
	boost::asio::awaitable<void> accept_loop();

	boost::asio::io_context &ioc_;
	boost::asio::ip::tcp::acceptor acceptor_;
	registry &registry_;

	static constexpr std::string COMPONENT {"coroutine_listener"};
};

} // namespace warp::http
