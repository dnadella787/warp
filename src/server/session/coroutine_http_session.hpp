#pragma once

#include <map>
#include <memory>
#include <optional>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "connection_close_policy.h"
#include "../router/registry.hpp"
#include "warp/warp.hpp"

namespace warp::server {

class coroutine_http_session : public std::enable_shared_from_this<coroutine_http_session> {
public:
	coroutine_http_session(boost::asio::ip::tcp::socket &&socket, registry &routes, log::logger logger);

	void start();

private:
	boost::asio::awaitable<void> read_loop();
	boost::asio::awaitable<void> write_loop();
	boost::asio::awaitable<void> wait_for_read_ready();
	boost::asio::awaitable<void> wait_for_write_ready();
	void execute_sync_handler(std::size_t sequence, const http::sync_handler &handler, http::request req);
	static boost::asio::awaitable<void> execute_async_handler(std::shared_ptr<coroutine_http_session> self,
	                                                          std::size_t sequence, const http::async_handler &handler,
	                                                          http::request req);
	void complete_request(std::size_t sequence, http::response response);
	void notify_read_loop();
	void notify_write_loop();
	void finish_request(std::size_t sequence);
	void shutdown();

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	registry &routes_;
	log::logger logger_;
	std::map<std::size_t, request_context> request_ctxs_;
	std::map<std::size_t, pending_write> pending_responses_;
	connection_close_policy policy_ {};
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;
	boost::asio::steady_timer read_signal_;
	boost::asio::steady_timer write_signal_;
	std::size_t next_request_sequence_ {0};
	std::size_t next_write_sequence_ {0};
	std::size_t outstanding_requests_ {0};
	bool stop_reading_ {false};
	bool shutdown_started_ {false};

	static constexpr std::size_t pipeline_limit_ {8};
	static constexpr std::string_view COMPONENT {"coroutine_http_session"};
};

} // namespace warp::server
