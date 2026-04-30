#include "coroutine_listener.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "server/session/coroutine_http_session.hpp"

namespace warp::server {

coroutine_listener::coroutine_listener(boost::asio::io_context &ioc, const registry &registry,
                                       const route_executor_table<http::event_loop_mode::coroutines> &route_executors,
                                       const interceptor_chain<request> &req_interceptor_chain,
                                       const interceptor_chain<response> &resp_interceptor_chain,
                                       const std::string &address, const unsigned short port, log::logger logger)
    : http_listener(ioc, registry, route_executors, req_interceptor_chain, resp_interceptor_chain, address, port,
                    std::move(logger)) {
}

void coroutine_listener::execute() {
	boost::asio::co_spawn(
	    acceptor_.get_executor(),
	    [self = shared_from_this()]() -> boost::asio::awaitable<void> {
		    // transfer execution to nested coroutine and suspends the outer lambda until it finishes (i.e. server side
		    // stop)
		    co_await self->accept_loop();
	    },
	    boost::asio::detached);
}

boost::asio::awaitable<void> coroutine_listener::accept_loop() {
	for (;;) {
		boost::beast::error_code ec;
		// suspends inner coroutine and return control to the io_context while async_accept is outstanding
		auto socket = co_await acceptor_.async_accept(boost::asio::make_strand(ioc_),
		                                              boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			// server side abort, just exit the loop and let the lambda in the coroutine complete
			// TODO: maybe alos check parent server_impl status before aborting
			if (ec == boost::asio::error::operation_aborted)
				co_return;
			logger_.error("Error in coroutine_listener during accept_loop: {}", ec.message());
			continue;
		}

		// create the session as another coroutine
		std::make_shared<coroutine_http_session>(std::move(socket), registry_, route_executors_, req_interceptor_chain_,
		                                         resp_interceptor_chain_, logger_)
		    ->start();
	}
}

} // namespace warp::server
