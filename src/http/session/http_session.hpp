#pragma once

#include <map>
#include <memory>
#include <optional>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "../router/registry.hpp"
#include "warp/warp.hpp"

namespace warp::http {

class http_session : public std::enable_shared_from_this<http_session> {
public:
	http_session(boost::asio::ip::tcp::socket &&socket, registry &routes);

	void start();

private:
	void do_read();
	void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
	void on_handler_complete(std::size_t sequence, unsigned version, bool keep_alive, std::exception_ptr eptr,
	                         warp::response response);
	void do_write();
	void on_write(std::size_t sequence, bool keep_alive, boost::beast::error_code ec, std::size_t bytes_transferred);
	void maybe_read();
	void maybe_write();
	void shutdown();

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	registry &routes_;
	std::map<std::size_t, warp::response> pending_responses_;
	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;
	// Don't need std::atomic bc the session is guaranteed serialization by a strand created by the listener
	std::size_t next_request_sequence_ {0}; // sequence number assigned to the next request accepted on this connection
	std::size_t next_write_sequence_ {0};   // sequence number of the next response that must be written
	std::size_t outstanding_requests_ {0};  // total requests in the pipeline not fully flushed to the client yet
	bool read_in_progress_ {false};         // true while an async_read is currently active on this session
	bool write_in_progress_ {false};        // true while an async_write is currently active on this session
	bool stop_reading_ {false};             // stop accepting new requests, but still drain already accepted ones
	bool shutdown_started_ {false};         // session shutdown has begun; do not start more reads or writes

	static constexpr std::size_t pipeline_limit_ {8};
	static constexpr std::string_view COMPONENT {"http_session"};
};

} // namespace warp::http
