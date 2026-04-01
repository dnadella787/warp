#pragma once

#include "warp/db/postgres/connection_config.hpp"
#include "warp/http/server.hpp"

#include <benchmark/benchmark.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace warp::benchmarks {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

inline constexpr std::uint64_t rss_sample_interval = 128;
inline constexpr double benchmark_min_time_seconds = 60.0;

struct db_env {
	std::string host;
	std::optional<std::uint16_t> port;
	std::string user;
	std::string password;
	std::string database;
};

warp::db::postgres::connection_config make_db_config(const db_env &env);
std::optional<db_env> load_db_env();

class process_resource_tracker {
public:
	void start();
	void sample_memory();
	void record(benchmark::State &state);

private:
	std::chrono::steady_clock::time_point wall_start_ {};
	std::optional<double> cpu_start_seconds_;
	std::optional<std::uint64_t> peak_rss_bytes_;
};

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
};

std::unique_ptr<client_connection> connect_client(std::uint16_t port);
void send_request(client_connection &client, std::string_view payload);
http::response<http::string_body> read_response(client_connection &client);
void close_connection(client_connection &client);
void run_round_trip_benchmark(benchmark::State &state, client_connection &client, std::string_view request_payload);

} // namespace warp::benchmarks
