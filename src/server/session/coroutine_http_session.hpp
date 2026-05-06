#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "policy/connection_close_policy.h"
#include "policy/transport.h"
#include "server/interceptors/interceptor_chain.h"
#include "server/listener/traits.hpp"
#include "warp/warp.hpp"

namespace warp::server {

template <warp_session_transport Transport>
class coroutine_http_session : public std::enable_shared_from_this<coroutine_http_session<Transport>> {
public:
	using stream_t = typename Transport::stream_type;
	using route_runtime_t = typename event_loop_traits<event_loop_mode::coroutines, Transport>::route_runtime_type;

	coroutine_http_session(boost::asio::ip::tcp::socket &&socket, Transport transport, const route_runtime_t &routes,
	                       const interceptor_chain<request> &req_chain, const interceptor_chain<response> &resp_chain,
	                       log::logger logger);

	void start();
	void dispatch_sync_handler(std::size_t sequence, const http::sync_handler &handler, http::request req);
	void dispatch_async_handler(std::size_t sequence, const http::async_handler &handler, http::request req);

private:
	boost::asio::awaitable<void> read_loop();
	boost::asio::awaitable<void> write_loop();
	boost::asio::awaitable<void> wait_for_read_ready();
	boost::asio::awaitable<void> wait_for_write_ready();
	void fail_transport_start(std::string_view stage, boost::beast::error_code ec);
	static boost::asio::awaitable<void> run_async_handler(std::shared_ptr<coroutine_http_session> self,
	                                                      std::size_t sequence, const http::async_handler &handler,
	                                                      http::request req);
	void complete_request(std::size_t sequence, http::response response);
	void notify_read_loop();
	void notify_write_loop();
	void finish_request(std::size_t sequence);
	void graceful_shutdown();
	void abort_transport();

	friend struct plain_session_transport;
	friend struct tls_session_transport;
	friend struct http_session_io_starter;

	[[no_unique_address]] Transport transport_;
	stream_t stream_;
	boost::beast::flat_buffer buffer_;
	const route_runtime_t &routes_;
	const interceptor_chain<request> &req_interceptor_chain_;
	const interceptor_chain<response> &resp_interceptor_chain_;
	log::logger logger_;
	std::map<std::size_t, request_context> request_ctxs_;
	std::map<std::size_t, pending_write> pending_responses_;
	connection_close_policy close_policy_ {};
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;
	boost::asio::steady_timer read_signal_;
	boost::asio::steady_timer write_signal_;
	std::size_t next_request_sequence_ {0};
	std::size_t next_write_sequence_ {0};
	std::size_t outstanding_requests_ {0};
	bool stop_reading_ {false};
	bool shutdown_started_ {false};

	static constexpr std::size_t pipeline_limit_ {8};
};

} // namespace warp::server

#include "coroutine_http_session.tpp"
