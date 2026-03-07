#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "warp/net/http/response.hpp"
#include "../../net/router/registry.hpp"

namespace warp::http::detail {

class session : public std::enable_shared_from_this<session> {
public:
	session(boost::asio::ip::tcp::socket socket, net::router::registry &routes);

	void start();

private:
	void read();
	void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
	void write_response(warp::net::http::response resp);
	void on_write(boost::beast::error_code ec, std::size_t bytes_transferred, bool close);
	void shutdown();

	boost::beast::tcp_stream stream_;
	boost::beast::flat_buffer buffer_;
	net::router::registry &routes_;
	boost::beast::http::request<boost::beast::http::string_body> request_;
};

} // namespace warp::http::detail
