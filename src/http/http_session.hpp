#pragma once

#include <memory>
#include <optional>
#include <queue>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "registry.hpp"
#include "warp/http/server.hpp"

namespace warp::http {

class http_session : public std::enable_shared_from_this<http_session> {
public:
	http_session(boost::asio::ip::tcp::socket &&socket, registry &routes);

	void start();

private:
	void do_read();
	void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
	void on_handler_complete(unsigned version, bool keep_alive, std::exception_ptr eptr, warp::response response);
	void queue_write(warp::response response);

	void do_write();

	void on_write(bool keep_alive, boost::beast::error_code ec, std::size_t bytes_transferred);
	void shutdown();

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	registry &routes_;
	std::queue<response> response_queue_;
	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;

	static constexpr std::string component {"http_session"};
};

} // namespace warp::http
