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
	void on_read(const boost::beast::error_code &ec, std::size_t bytes_transferred);
	void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
	void write_response(const net::http::response &resp);
	void on_write(boost::beast::error_code ec, std::size_t bytes_transferred, bool close);
	void shutdown();

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	net::router::registry &routes_;
	boost::beast::http::request<boost::beast::http::string_body> request_;
	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	std::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;

	constexpr std::string component {"http_session"};
};

} // namespace warp::http::detail
