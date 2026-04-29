#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <optional>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "connection_close_policy.h"
#include "server/router/registry.hpp"
#include "warp/warp.hpp"

namespace warp::server {

class callback_http_session : public std::enable_shared_from_this<callback_http_session> {
public:
	callback_http_session(boost::asio::ip::tcp::socket &&socket, registry &routes, log::logger logger);

	void start();

private:
	void maybe_read();
	void do_read();
	void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
	void on_handler_complete(std::size_t sequence, std::exception_ptr eptr, warp::response response);
	void maybe_write();
	void do_write();
	void on_write(std::size_t sequence, boost::beast::error_code ec, std::size_t bytes_transferred);
	void finish_request(std::size_t sequence);
	void shutdown(bool force = false);

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	registry &routes_;
	log::logger logger_;
	std::map<std::size_t, request_context> request_ctxs_;
	std::map<std::size_t, pending_write> pending_responses_;
	connection_close_policy close_policy_;
	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;
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
	static constexpr std::string_view COMPONENT {"http_session"};
};

} // namespace warp::server
