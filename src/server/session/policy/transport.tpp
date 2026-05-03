#pragma once

#include <chrono>
#include <type_traits>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/bind_handler.hpp>

namespace warp::server {

[[nodiscard]] inline bool ignore_tls_shutdown_error(const beast::error_code &ec) {
	return ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted ||
	       ec == boost::asio::error::not_connected || ec == ssl::error::stream_truncated;
}

template <callback_event_loop_http_session Session>
void http_session_io_starter::start(Session &session) {
	boost::asio::dispatch(
	    session.stream_.get_executor(),
	    beast::bind_front_handler(&Session::do_read, session.shared_from_this()));
}

template <coroutine_event_loop_http_session Session>
void http_session_io_starter::start(Session &session) {
	boost::asio::co_spawn(
	    session.stream_.get_executor(),
	    [self = session.shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->read_loop(); },
	    boost::asio::detached);
	boost::asio::co_spawn(
	    session.stream_.get_executor(),
	    [self = session.shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->write_loop(); },
	    boost::asio::detached);
}

template <warp_http_session Session>
void plain_session_transport::start(Session &session) {
	http_session_io_starter::start(session);
}

template <warp_http_session Session>
void plain_session_transport::graceful_shutdown(Session &session) {
	abort(session.stream_);
}

template <warp_http_session Session>
void tls_session_transport::start(Session &session) {
	boost::asio::dispatch(session.stream_.get_executor(), [self = session.shared_from_this()]() {
		beast::get_lowest_layer(self->stream_).expires_after(std::chrono::seconds(30));
		self->stream_.async_handshake(ssl::stream_base::server, [self](beast::error_code ec) {
			if (ec) {
				self->fail_transport_start("TLS handshake", ec);
				return;
			}

			http_session_io_starter::start(*self);
		});
	});
}

template <warp_http_session Session>
void tls_session_transport::graceful_shutdown(Session &session) {
	// 5 sec timeout for peer close_notify.
	auto self = session.shared_from_this();
	beast::get_lowest_layer(self->stream_).expires_after(std::chrono::seconds(5));
	self->stream_.async_shutdown([self](beast::error_code ec) {
		if (ec && !ignore_tls_shutdown_error(ec)) {
			self->logger_.trace("error in session during shutdown: {}", ec.message());
		}
	});
}

} // namespace warp::server
