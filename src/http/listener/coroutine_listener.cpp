#include "coroutine_listener.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "../../common/util/fail.h"
#include "../session/coroutine_http_session.hpp"

namespace warp::http {

coroutine_listener::coroutine_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
                                       const unsigned short port)
    : base_listener(ioc, registry, address, port) {
}

void coroutine_listener::run() {
	boost::asio::co_spawn(
	    acceptor_.get_executor(),
	    [self = shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->accept_loop(); },
	    boost::asio::detached);
}

boost::asio::awaitable<void> coroutine_listener::accept_loop() {
	for (;;) {
		boost::beast::error_code ec;
		auto socket = co_await acceptor_.async_accept(boost::asio::make_strand(ioc_),
		                                              boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			if (ec == boost::asio::error::operation_aborted) {
				co_return;
			}
			util::fail(ec, COMPONENT, "accept_loop");
			continue;
		}

		std::make_shared<coroutine_http_session>(std::move(socket), registry_)->start();
	}
}

} // namespace warp::http
