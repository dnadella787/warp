#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "policy/connection_close_policy.h"
#include "policy/transport.h"
#include "server/interceptors/interceptor_chain.h"
#include "server/listener/traits.hpp"
#include "server/router/registry.hpp"
#include "warp/warp.hpp"

namespace warp::server {

namespace beast = boost::beast;

template <warp_session_transport Transport>
class callback_http_session : public std::enable_shared_from_this<callback_http_session<Transport>> {
public:
	using stream_t = typename Transport::stream_type;
	using executor_table_t = typename event_loop_traits<event_loop_mode::callbacks, Transport>::executor_table_type;

	callback_http_session(boost::asio::ip::tcp::socket &&socket, Transport transport, const registry &routes,
	                      const executor_table_t &route_executors, const interceptor_chain<request> &req_chain,
	                      const interceptor_chain<response> &resp_chain, log::logger logger);

	void start();
	void dispatch_sync_handler(std::size_t sequence, const http::sync_handler &handler, http::request request);
	void dispatch_async_handler(std::size_t sequence, const http::async_handler &handler, http::request request);

private:
	void maybe_read();
	void do_read();
	void on_read(beast::error_code ec, std::size_t bytes_transferred);
	void fail_transport_start(std::string_view stage, beast::error_code ec);

	static boost::asio::awaitable<http::response>
	run_async_handler(std::shared_ptr<callback_http_session<Transport>> self, const http::async_handler &handler,
	                  http::request req);

	void on_handler_complete(std::size_t sequence, std::exception_ptr eptr, response response);
	void maybe_write();
	void do_write();
	void on_write(std::size_t sequence, beast::error_code ec, std::size_t bytes_transferred);
	void finish_request(std::size_t sequence);
	void graceful_shutdown();
	void abort_transport();

	friend struct plain_session_transport;
	friend struct tls_session_transport;
	friend struct http_session_io_starter;

	[[no_unique_address]] Transport transport_;
	stream_t stream_;
	beast::flat_buffer buffer_;
	const registry &routes_;
	const executor_table_t &route_executors_;
	const interceptor_chain<request> &req_interceptor_chain_;
	const interceptor_chain<response> &resp_interceptor_chain_;
	log::logger logger_;
	std::map<std::size_t, request_context> request_ctxs_;
	std::map<std::size_t, pending_write> pending_responses_;
	connection_close_policy close_policy_;
	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	std::optional<beast::http::request_parser<beast::http::string_body>> parser_;
	// Don't need std::atomic bc the session is guaranteed serialization by a strand created by the listener
	std::size_t next_request_sequence_ {0}; // sequence number assigned to the next request accepted on this connection
	std::size_t next_write_sequence_ {0};   // sequence number of the next response that must be written
	std::size_t outstanding_requests_ {0};  // total requests in the pipeline not fully flushed to the client yet (this
	                                        // is not just the writes in queue, it includes in flight request handlers)
	bool read_in_progress_ {false};         // true while an async_read is currently active on this session
	bool write_in_progress_ {false};        // true while an async_write is currently active on this session
	bool stop_reading_ {false};             // stop accepting new requests, but still drain already accepted ones
	bool shutdown_started_ {false};         // session shutdown has begun; do not start more reads or writes
	static constexpr std::size_t pipeline_limit_ {8};
};

} // namespace warp::server

#include "callback_http_session.tpp"
