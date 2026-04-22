#pragma once

#include "warp/http/server.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "warp/warp.hpp"
#include "warp/http/server_builder.hpp"

namespace warp::tests::integration_support {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using http_response = http::response<http::string_body>;

struct server_fixture {
	explicit server_fixture(warp::http::server_builder builder);

	template <event_loop_mode Mode>
	explicit server_fixture(warp::http::server_builder builder, std::integral_constant<event_loop_mode, Mode>)
	    : port(reserve_port()),
	      server(builder.address("127.0.0.1").port(port).worker_threads(4).template build<Mode>()) {
		server.run(false);
	}

	~server_fixture();

	std::uint16_t port;
	warp::http::server server;

private:
	static std::uint16_t reserve_port();
};

template <event_loop_mode Mode>
using event_loop_mode_tag = std::integral_constant<event_loop_mode, Mode>;

struct client_connection {
	asio::io_context ioc;
	beast::tcp_stream stream {ioc};
	beast::flat_buffer buffer;

	client_connection() = default;
	client_connection(const client_connection &) = delete;
	client_connection &operator=(const client_connection &) = delete;
	client_connection(client_connection &&) = delete;
	client_connection &operator=(client_connection &&) = delete;
};

std::string make_get_request(std::string_view path, std::string_view connection = "keep-alive");
std::unique_ptr<client_connection> connect_client(std::uint16_t port);
void send_requests(client_connection &client, std::string_view payload);
http_response read_response(client_connection &client);
bool next_response_is_eof(client_connection &client);
bool read_until_eof(client_connection &client);
boost::json::object parse_object_body(const http_response &response);

asio::awaitable<response> delayed_ok_response(std::chrono::milliseconds delay, std::function<std::string()> body_fn);

const char *event_loop_mode_name(event_loop_mode mode);

} // namespace warp::tests::integration_support
