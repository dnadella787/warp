#include "coroutine_listener.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "server/session/coroutine_http_session.hpp"

namespace warp::server {

template <warp_session_transport Transport>
coroutine_listener<Transport>::coroutine_listener(
    boost::asio::io_context &ioc, transport_provider<Transport> transport_provider, const registry &registry,
    const std::vector<http::handler> &route_handlers, const interceptor_chain<request> &req_interceptor_chain,
    const interceptor_chain<response> &resp_interceptor_chain, const std::string &address, const unsigned short port,
    log::logger logger)
    : base_type(ioc, std::move(transport_provider), registry, route_handlers, req_interceptor_chain,
                resp_interceptor_chain, address, port, std::move(logger)) {
}

template <warp_session_transport Transport>
void coroutine_listener<Transport>::run() {
	boost::asio::co_spawn(
	    this->acceptor_.get_executor(),
	    [self = this->shared_from_this()]() -> boost::asio::awaitable<void> {
		    // transfer execution to nested coroutine and suspends the outer lambda until it finishes (i.e. server side
		    // stop)
		    co_await self->accept_loop();
	    },
	    boost::asio::detached);
}

template <warp_session_transport Transport>
boost::asio::awaitable<void> coroutine_listener<Transport>::accept_loop() {
	for (;;) {
		boost::beast::error_code ec;
		// suspends inner coroutine and return control to the io_context while async_accept is outstanding
		auto socket = co_await this->acceptor_.async_accept(
		    boost::asio::make_strand(this->ioc_), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			// server side abort, just exit the loop and let the lambda in the coroutine complete
			// TODO: maybe alos check parent server_impl status before aborting
			if (ec == boost::asio::error::operation_aborted)
				co_return;
			this->logger_.error("error in coroutine_listener during accept_loop: {}", ec.message());
			continue;
		}

		// create the session as another coroutine
		this->start_session(std::move(socket));
	}
}

template class coroutine_listener<plain_session_transport>;
template class coroutine_listener<tls_session_transport>;
} // namespace warp::server
