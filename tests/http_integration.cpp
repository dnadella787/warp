#include "warp/http/server.hpp"
#include "warp/db/postgres/connection_config.hpp"
#include "warp/db/postgres/connection_pool.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json/parse.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

struct server_fixture {
	explicit server_fixture(warp::http::server_builder builder)
	    : port(reserve_port()), server(builder.address("127.0.0.1").port(port).worker_threads(4).build()) {
		server.run(false);
	}

	~server_fixture() {
		server.stop();
	}

	std::uint16_t port;
	warp::http::server server;

private:
	static std::uint16_t reserve_port() {
		asio::io_context ioc;
		tcp::acceptor acceptor(ioc, tcp::endpoint {tcp::v4(), 0});
		return acceptor.local_endpoint().port();
	}
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

constexpr std::array<warp::event_loop_mode, 2> event_loop_modes {
    warp::event_loop_mode::callbacks,
    warp::event_loop_mode::coroutines,
};

std::string make_request(std::string_view path, std::string_view connection = "keep-alive") {
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
	auto deadline = std::chrono::steady_clock::now() + 5s;
	beast::error_code ec;

	for (;;) {
		client->stream.socket().close(ec);
		client->stream.connect(endpoint, ec);
		if (!ec) {
			return client;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			throw std::runtime_error("timed out connecting to test server");
		}
		std::this_thread::sleep_for(20ms);
	}
}

void send_requests(client_connection &client, const std::string &payload) {
	client.stream.expires_after(5s);
	asio::write(client.stream.socket(), asio::buffer(payload));
}

http::response<http::string_body> read_response(client_connection &client) {
	http::response<http::string_body> response;
	client.stream.expires_after(5s);
	http::read(client.stream, client.buffer, response);
	return response;
}

bool read_until_eof(client_connection &client) {
	std::array<char, 512> scratch {};
	client.stream.expires_after(5s);
	beast::error_code ec;
	for (;;) {
		auto bytes = client.stream.socket().read_some(asio::buffer(scratch), ec);
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

boost::json::object parse_object_body(const http::response<http::string_body> &response) {
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

asio::awaitable<warp::response> delay_ok(std::string label, std::chrono::milliseconds delay,
                                         std::function<std::string()> body_fn) {
	auto executor = co_await asio::this_coro::executor;
	asio::steady_timer timer(executor);
	timer.expires_after(delay);
	co_await timer.async_wait(asio::use_awaitable);
	co_return warp::response::ok(body_fn());
}

void test_pipelined_ordering(warp::event_loop_mode mode) {
	auto slow_started = std::make_shared<std::atomic<bool>>(false);
	auto fast_finished = std::make_shared<std::atomic<bool>>(false);

	server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(mode)
	        .get("/slow",
	             [slow_started, fast_finished](warp::request) -> warp::awaitable<warp::response> {
		             slow_started->store(true, std::memory_order_release);
		             co_return co_await delay_ok("slow", 150ms, [fast_finished]() {
			             return warp::body_builder()
			                 .set("route", "slow")
			                 .set("fast_finished_before_return", fast_finished->load(std::memory_order_acquire))
			                 .build();
		             });
	             })
	        .get("/fast", [slow_started, fast_finished](const warp::request &) -> warp::response {
		        fast_finished->store(true, std::memory_order_release);
		        return warp::response::ok(warp::body_builder()
		                                      .set("route", "fast")
		                                      .set("saw_slow_started", slow_started->load(std::memory_order_acquire))
		                                      .build());
	        }));

	auto client = connect_client(fixture.port);
	send_requests(*client, make_request("/slow") + make_request("/fast", "close"));

	auto slow = read_response(*client);
	auto fast = read_response(*client);

	auto slow_body = parse_object_body(slow);
	auto fast_body = parse_object_body(fast);
	assert(slow.result() == http::status::ok);
	assert(fast.result() == http::status::ok);
	assert(slow_body.at("route").as_string() == "slow");
	assert(slow_body.at("fast_finished_before_return").as_bool());
	assert(fast_body.at("route").as_string() == "fast");
	assert(fast_body.at("saw_slow_started").as_bool());
	assert(read_until_eof(*client));
}

void test_many_pipelined_requests(warp::event_loop_mode mode) {
	server_fixture fixture(
	    warp::http::server_builder().event_loop(mode).get("/echo/{id}", [](const warp::request &req) -> warp::response {
		    return warp::response::ok(
		        warp::body_builder().set("id", std::string(req.path_param("id").value_or(""))).build());
	    }));

	std::string payload;
	for (int i = 0; i < 10; ++i) {
		payload += make_request("/echo/" + std::to_string(i), i == 9 ? "close" : "keep-alive");
	}

	auto client = connect_client(fixture.port);
	send_requests(*client, payload);

	for (int i = 0; i < 10; ++i) {
		auto response = read_response(*client);
		auto body = parse_object_body(response);
		assert(response.result() == http::status::ok);
		assert(body.at("id").as_string() == std::to_string(i));
	}
	assert(read_until_eof(*client));
}

void test_slow_third_request_blocks_later_writes(warp::event_loop_mode mode) {
	auto fast_after_three = std::make_shared<std::atomic<int>>(0);

	server_fixture fixture(warp::http::server_builder().event_loop(mode).get(
	    "/item/{id}", [fast_after_three](warp::request req) -> warp::awaitable<warp::response> {
		    auto id = std::string(req.path_param("id").value_or(""));
		    if (id == "3") {
			    co_return co_await delay_ok("three", 150ms, [fast_after_three, id]() {
				    return warp::body_builder()
				        .set("id", id)
				        .set("later_fast_finished", fast_after_three->load(std::memory_order_acquire) >= 5)
				        .build();
			    });
		    }

		    if (std::stoi(id) > 3) {
			    fast_after_three->fetch_add(1, std::memory_order_acq_rel);
		    }

		    co_return warp::response::ok(warp::body_builder().set("id", id).build());
	    }));

	std::string payload;
	for (int i = 1; i <= 8; ++i) {
		payload += make_request("/item/" + std::to_string(i), i == 8 ? "close" : "keep-alive");
	}

	auto client = connect_client(fixture.port);
	send_requests(*client, payload);

	for (int i = 1; i <= 8; ++i) {
		auto response = read_response(*client);
		auto body = parse_object_body(response);
		assert(response.result() == http::status::ok);
		assert(body.at("id").as_string() == std::to_string(i));
		if (i == 3) {
			assert(body.at("later_fast_finished").as_bool());
		}
	}
	assert(read_until_eof(*client));
}

void test_connection_close_stops_following_requests(warp::event_loop_mode mode) {
	auto after_processed = std::make_shared<std::atomic<int>>(0);

	server_fixture fixture(warp::http::server_builder()
	                           .event_loop(mode)
	                           .get("/close",
	                                [](const warp::request &) -> warp::response {
		                                return warp::response::ok(warp::body_builder().set("route", "close").build());
	                                })
	                           .get("/after", [after_processed](const warp::request &) -> warp::response {
		                           after_processed->fetch_add(1, std::memory_order_acq_rel);
		                           return warp::response::ok(warp::body_builder().set("route", "after").build());
	                           }));

	auto client = connect_client(fixture.port);
	send_requests(*client, make_request("/close", "close") + make_request("/after", "close"));

	auto response = read_response(*client);
	auto body = parse_object_body(response);
	assert(response.result() == http::status::ok);
	assert(body.at("route").as_string() == "close");
	assert(read_until_eof(*client));
	std::this_thread::sleep_for(100ms);
	assert(after_processed->load(std::memory_order_acquire) == 0);
}

void test_throwing_route_preserves_order(warp::event_loop_mode mode) {
	auto throw_started = std::make_shared<std::atomic<bool>>(false);

	server_fixture fixture(warp::http::server_builder()
	                           .event_loop(mode)
	                           .get("/throw",
	                                [throw_started](warp::request) -> warp::awaitable<warp::response> {
		                                throw_started->store(true, std::memory_order_release);
		                                auto executor = co_await asio::this_coro::executor;
		                                asio::steady_timer timer(executor);
		                                timer.expires_after(150ms);
		                                co_await timer.async_wait(asio::use_awaitable);
		                                throw std::runtime_error("boom");
	                                })
	                           .get("/fast", [throw_started](const warp::request &) -> warp::response {
		                           return warp::response::ok(
		                               warp::body_builder()
		                                   .set("route", "fast")
		                                   .set("saw_throw_started", throw_started->load(std::memory_order_acquire))
		                                   .build());
	                           }));

	auto client = connect_client(fixture.port);
	send_requests(*client, make_request("/throw") + make_request("/fast", "close"));

	auto first = read_response(*client);
	auto second = read_response(*client);
	auto second_body = parse_object_body(second);

	assert(first.result() == http::status::internal_server_error);
	assert(second.result() == http::status::ok);
	assert(second_body.at("route").as_string() == "fast");
	assert(second_body.at("saw_throw_started").as_bool());
	assert(read_until_eof(*client));
}

void test_missing_and_normal_routes_preserve_order(warp::event_loop_mode mode) {
	auto fast_finished = std::make_shared<std::atomic<bool>>(false);

	server_fixture fixture(warp::http::server_builder()
	                           .event_loop(mode)
	                           .get("/slow",
	                                [fast_finished](warp::request) -> warp::awaitable<warp::response> {
		                                co_return co_await delay_ok("slow", 150ms, [fast_finished]() {
			                                return warp::body_builder()
			                                    .set("route", "slow")
			                                    .set("fast_finished_before_return",
			                                         fast_finished->load(std::memory_order_acquire))
			                                    .build();
		                                });
	                                })
	                           .get("/fast", [fast_finished](const warp::request &) -> warp::response {
		                           fast_finished->store(true, std::memory_order_release);
		                           return warp::response::ok(warp::body_builder().set("route", "fast").build());
	                           }));

	auto client = connect_client(fixture.port);
	send_requests(*client, make_request("/slow") + make_request("/missing") + make_request("/fast", "close"));

	auto first = read_response(*client);
	auto second = read_response(*client);
	auto third = read_response(*client);

	auto first_body = parse_object_body(first);
	auto second_body = parse_object_body(second);
	auto third_body = parse_object_body(third);

	assert(first.result() == http::status::ok);
	assert(first_body.at("route").as_string() == "slow");
	assert(first_body.at("fast_finished_before_return").as_bool());
	assert(second.result() == http::status::not_found);
	assert(second_body.at("error").as_string() == "Not Found");
	assert(third.result() == http::status::ok);
	assert(third_body.at("route").as_string() == "fast");
	assert(read_until_eof(*client));
}

void test_db_route_if_configured(warp::event_loop_mode mode) {
	auto env = load_db_env();
	if (!env) {
		std::cerr << "Skipping DB integration test: WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME not set\n";
		return;
	}

	auto db_pool =
	    std::make_shared<warp::db::postgres::connection_pool>(asio::system_executor {}, make_db_config(*env), 4, 2);

	server_fixture fixture(warp::http::server_builder().event_loop(mode).get(
	    "/db/{id}", [db_pool](warp::request req) -> warp::awaitable<warp::response> {
		    auto id = std::string(req.path_param("id").value_or("0"));
		    auto result = co_await db_pool->query(std::string("select ") + id +
		                                          "::int as requested_id, current_database() as database_name");
		    co_return warp::response::ok(
		        warp::body_builder()
		            .set("requested_id", result.rows() > 0 ? std::string(result.value(0, 0)) : id)
		            .set("database_name", result.rows() > 0 ? std::string(result.value(0, 1)) : std::string {})
		            .build());
	    }));

	auto client = connect_client(fixture.port);
	send_requests(*client, make_request("/db/7", "close"));

	auto response = read_response(*client);
	auto body = parse_object_body(response);
	assert(response.result() == http::status::ok);
	assert(body.at("requested_id").as_string() == "7");
	assert(!body.at("database_name").as_string().empty());
	assert(read_until_eof(*client));
}

} // namespace

int main() {
	for (auto mode : event_loop_modes) {
		test_pipelined_ordering(mode);
		test_many_pipelined_requests(mode);
		test_slow_third_request_blocks_later_writes(mode);
		test_connection_close_stops_following_requests(mode);
		test_throwing_route_preserves_order(mode);
		test_missing_and_normal_routes_preserve_order(mode);
		test_db_route_if_configured(mode);
	}
	return 0;
}
