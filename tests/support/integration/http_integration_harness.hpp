#pragma once

#include "warp/db/postgres/connection_config.hpp"
#include "warp/http/server.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace warp::tests::integration_support {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using http_response = http::response<http::string_body>;

struct server_fixture {
	explicit server_fixture(warp::http::server_builder builder);
	~server_fixture();

	std::uint16_t port;
	warp::http::server server;

private:
	static std::uint16_t reserve_port();
};

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

struct db_env {
	std::string host;
	std::optional<std::uint16_t> port;
	std::string user;
	std::string password;
	std::string database;
};

std::string make_get_request(std::string_view path, std::string_view connection = "keep-alive");
std::unique_ptr<client_connection> connect_client(std::uint16_t port);
void send_requests(client_connection &client, std::string_view payload);
http_response read_response(client_connection &client);
bool read_until_eof(client_connection &client);
boost::json::object parse_object_body(const http_response &response);

warp::db::postgres::connection_config make_db_config(const db_env &env);
std::optional<db_env> load_db_env();

asio::awaitable<warp::response> delayed_ok_response(std::chrono::milliseconds delay,
                                                    std::function<std::string()> body_fn);

const char *event_loop_mode_name(warp::event_loop_mode mode);

} // namespace warp::tests::integration_support
