#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "warp/net/http/response.hpp"
#include "../../net/router/registry.hpp"

namespace warp::http::detail {

class http_session : public std::enable_shared_from_this<http_session> {
public:
	http_session(boost::asio::ip::tcp::socket &&socket, net::router::registry &routes);

	void start();

private:
	void do_read();
	void on_read(boost::beast::error_code &ec, std::size_t bytes_transferred);
	void queue_write(net::http::response response);
	void write_response(const net::http::response &resp);

	void do_write();

	void on_write(std::size_t bytes_transferred, bool keep_alive, boost::beast::error_code ec);
	void shutdown();

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	net::router::registry &routes_;
	std::queue<net::http::response> response_queue_;
	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;

	static constexpr std::string component {"http_session"};
	static constexpr std::size_t queue_limit = 8;
};

} // namespace warp::http::detail
