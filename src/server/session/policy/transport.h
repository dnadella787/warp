#pragma once

#include <cstddef>
#include <concepts>
#include <memory>
#include <type_traits>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>

#include "warp/http/event_loop_mode.hpp"
#include "warp/http/http.hpp"

namespace warp::server {

namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

template <typename Transport>
concept warp_session_transport = requires(const Transport &transport, tcp::socket &&socket,
                                          typename Transport::stream_type &stream) {
	typename Transport::stream_type;
	{ transport.make_stream(std::move(socket)) } -> std::same_as<typename Transport::stream_type>;
	{ Transport::abort(stream) } -> std::same_as<void>;
};

template <warp_session_transport Transport>
class callback_http_session;

template <warp_session_transport Transport>
class coroutine_http_session;

template <typename Session>
struct http_session_traits;

template <warp_session_transport Transport>
struct http_session_traits<callback_http_session<Transport>> {
	using transport_type = Transport;
	static constexpr auto event_loop_mode = http::event_loop_mode::callbacks;
};

template <warp_session_transport Transport>
struct http_session_traits<coroutine_http_session<Transport>> {
	using transport_type = Transport;
	static constexpr auto event_loop_mode = http::event_loop_mode::coroutines;
};

template <typename Session>
concept warp_http_session = requires(Session &session, std::size_t sequence, const http::sync_handler &sync_handler,
                                     const http::async_handler &async_handler, http::request request) {
	typename http_session_traits<std::remove_cvref_t<Session>>::transport_type;
	{ session.start() } -> std::same_as<void>;
	{ session.dispatch_sync_handler(sequence, sync_handler, std::move(request)) } -> std::same_as<void>;
	{ session.dispatch_async_handler(sequence, async_handler, std::move(request)) } -> std::same_as<void>;
};

template <warp_http_session Session>
inline constexpr auto http_session_event_loop_mode_v =
    http_session_traits<std::remove_cvref_t<Session>>::event_loop_mode;

template <typename Session>
concept callback_event_loop_http_session =
    warp_http_session<Session> &&
    http_session_event_loop_mode_v<Session> == http::event_loop_mode::callbacks;

template <typename Session>
concept coroutine_event_loop_http_session =
    warp_http_session<Session> &&
    http_session_event_loop_mode_v<Session> == http::event_loop_mode::coroutines;

struct http_session_io_starter {
	template <callback_event_loop_http_session Session>
	static void start(Session &session);

	template <coroutine_event_loop_http_session Session>
	static void start(Session &session);
};

struct plain_session_transport {
	using stream_type = beast::tcp_stream;

	[[nodiscard]] stream_type make_stream(tcp::socket &&socket) const;
	template <warp_http_session Session>
	static void start(Session &session);
	template <warp_http_session Session>
	static void graceful_shutdown(Session &session);
	static void abort(stream_type &stream);
};

struct tls_session_transport {
	using stream_type = ssl::stream<beast::tcp_stream>;

	explicit tls_session_transport(std::shared_ptr<ssl::context> ctx);

	[[nodiscard]] stream_type make_stream(tcp::socket &&socket) const;
	template <warp_http_session Session>
	static void start(Session &session);
	template <warp_http_session Session>
	static void graceful_shutdown(Session &session);
	static void abort(stream_type &stream);

private:
	std::shared_ptr<ssl::context> ctx_;
};

} // namespace warp::server

#include "transport.tpp"
