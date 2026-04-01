#include "http_integration_harness.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/json/parse.hpp>

#include <array>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace warp::tests::integration_support {

using namespace std::chrono_literals;

server_fixture::server_fixture(warp::http::server_builder builder)
    : port(reserve_port()), server(builder.address("127.0.0.1").port(port).worker_threads(4).build()) {
	server.run(false);
}

server_fixture::~server_fixture() {
	server.stop();
}

std::uint16_t server_fixture::reserve_port() {
	asio::io_context ioc;
	tcp::acceptor acceptor(ioc, tcp::endpoint {tcp::v4(), 0});
	return acceptor.local_endpoint().port();
}

std::string make_get_request(std::string_view path, std::string_view connection) {
	return "GET " + std::string(path) +
	       " HTTP/1.1\r\n"
	       "Host: 127.0.0.1\r\n"
	       "Connection: " +
	       std::string(connection) +
	       "\r\n"
	       "\r\n";
}

std::unique_ptr<client_connection> connect_client(std::uint16_t port) {
	auto client = std::make_unique<client_connection>();
	client->stream.expires_after(5s);

	const auto endpoint = tcp::endpoint {asio::ip::make_address("127.0.0.1"), port};
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	beast::error_code ec;

	for (;;) {
		client->stream.socket().close(ec);
		client->stream.connect(endpoint, ec);
		if (!ec) {
			return client;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			throw std::runtime_error("timed out connecting to integration test server");
		}
		std::this_thread::sleep_for(20ms);
	}
}

void send_requests(client_connection &client, std::string_view payload) {
	client.stream.expires_after(5s);
	asio::write(client.stream.socket(), asio::buffer(payload.data(), payload.size()));
}

http_response read_response(client_connection &client) {
	http_response response;
	client.stream.expires_after(5s);
	http::read(client.stream, client.buffer, response);
	return response;
}

bool read_until_eof(client_connection &client) {
	std::array<char, 512> scratch {};
	client.stream.expires_after(5s);
	beast::error_code ec;
	for (;;) {
		const auto bytes = client.stream.socket().read_some(asio::buffer(scratch), ec);
		if (ec == asio::error::eof || ec == beast::http::error::end_of_stream) {
			return true;
		}
		if (ec) {
			return false;
		}
		if (bytes == 0) {
			return true;
		}
	}
}

boost::json::object parse_object_body(const http_response &response) {
	return boost::json::parse(response.body()).as_object();
}

warp::db::postgres::connection_config make_db_config(const db_env &env) {
	warp::db::postgres::connection_config config;
	config.host = env.host.empty() ? "127.0.0.1" : env.host;
	config.port = env.port;
	config.user = env.user;
	config.password = env.password;
	config.database = env.database;
	return config;
}

std::optional<db_env> load_db_env() {
	const char *user = std::getenv("WARP_DB_USER");
	const char *password = std::getenv("WARP_DB_PASSWORD");
	const char *database = std::getenv("WARP_DB_NAME");
	if (user == nullptr || password == nullptr || database == nullptr) {
		return std::nullopt;
	}

	db_env env;
	env.user = user;
	env.password = password;
	env.database = database;
	if (const char *host = std::getenv("WARP_DB_HOST")) {
		env.host = host;
	}
	if (const char *port = std::getenv("WARP_DB_PORT")) {
		env.port = static_cast<std::uint16_t>(std::stoi(port));
	}
	return env;
}

asio::awaitable<warp::response> delayed_ok_response(std::chrono::milliseconds delay,
                                                    std::function<std::string()> body_fn) {
	const auto executor = co_await asio::this_coro::executor;
	asio::steady_timer timer(executor);
	timer.expires_after(delay);
	co_await timer.async_wait(asio::use_awaitable);
	co_return warp::response::ok(body_fn());
}

const char *event_loop_mode_name(warp::event_loop_mode mode) {
	switch (mode) {
	case warp::event_loop_mode::callbacks:
		return "Callbacks";
	case warp::event_loop_mode::coroutines:
		return "Coroutines";
	}
	return "Unknown";
}

} // namespace warp::tests::integration_support
