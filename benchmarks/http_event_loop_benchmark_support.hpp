#pragma once

#include "warp/db/postgres/connection_config.hpp"
#include "warp/server/server_builder.hpp"

#include <benchmark/benchmark.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace warp::benchmarks {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

inline constexpr double benchmark_min_time_seconds = 60.0;
inline constexpr std::uint64_t rss_sample_interval = 128;
inline constexpr std::size_t default_benchmark_concurrency = 1024;
inline constexpr std::size_t default_benchmark_client_threads = 4;
inline constexpr std::chrono::milliseconds default_benchmark_warmup {5'000};
inline constexpr std::chrono::milliseconds default_benchmark_duration {60'000};
inline constexpr std::chrono::milliseconds default_benchmark_connect_timeout {5'000};
inline constexpr std::chrono::milliseconds default_benchmark_request_timeout {5'000};

struct db_env {
	std::string host;
	std::optional<std::uint16_t> port;
	std::string user;
	std::string password;
	std::string database;
};

db::postgres::connection_config make_db_config(const db_env &env);
std::optional<db_env> load_db_env();

class process_resource_tracker {
public:
	void start();
	void stop();
	void sample_memory();
	void record(benchmark::State &state, std::uint64_t requests_processed) const;

	[[nodiscard]] double cpu_seconds() const;
	[[nodiscard]] double wall_seconds() const;
	[[nodiscard]] std::optional<double> peak_rss_mib() const;

private:
	std::chrono::steady_clock::time_point wall_start_ {};
	std::optional<double> cpu_start_seconds_;
	std::optional<double> cpu_end_seconds_;
	std::optional<double> wall_elapsed_seconds_;
	std::optional<std::uint64_t> peak_rss_bytes_;
};

struct load_test_options {
	std::size_t concurrency {default_benchmark_concurrency};
	std::size_t client_threads {default_benchmark_client_threads};
	std::chrono::milliseconds warmup {default_benchmark_warmup};
	std::chrono::milliseconds duration {default_benchmark_duration};
	std::chrono::milliseconds connect_timeout {default_benchmark_connect_timeout};
	std::chrono::milliseconds request_timeout {default_benchmark_request_timeout};
	std::chrono::milliseconds sample_period {std::chrono::milliseconds {100}};
};

struct load_test_metrics {
	std::uint64_t successful_requests {0};
	std::uint64_t failed_requests {0};
	std::uint64_t completed_responses {0};
	std::uint64_t connect_errors {0};
	std::uint64_t write_errors {0};
	std::uint64_t read_errors {0};
	std::uint64_t response_status_errors {0};
	std::uint64_t latency_samples_observed {0};
	std::uint64_t latency_samples_kept {0};
	double throughput_rps {0.0};
	double error_rate_pct {0.0};
	double latency_p50_us {0.0};
	double latency_p90_us {0.0};
	double latency_p99_us {0.0};
	double latency_max_us {0.0};
};

struct server_fixture {
	explicit server_fixture(server::server_builder builder);

	template <http::event_loop_mode Mode>
	explicit server_fixture(server::server_builder builder, std::integral_constant<http::event_loop_mode, Mode>)
	    : port(reserve_port()),
	      server(builder.address("127.0.0.1").port(port).worker_threads(4).template build<Mode>()) {
		server.run(false);
	}

	~server_fixture();

	std::uint16_t port;
	server::server server;

private:
	static std::uint16_t reserve_port();
};

template <http::event_loop_mode Mode>
using event_loop_mode_tag = std::integral_constant<http::event_loop_mode, Mode>;

struct client_connection {
	asio::io_context ioc;
	beast::tcp_stream stream {ioc};
	beast::flat_buffer buffer;
};

const load_test_options &benchmark_load_test_options();
void set_benchmark_load_test_options(load_test_options options);
const std::vector<std::size_t> &benchmark_concurrency_levels();
load_test_options load_test_options_for_concurrency(std::size_t concurrency);

std::string format_duration(std::chrono::milliseconds duration);
std::string format_load_test_configuration(std::size_t effective_client_threads);
bool parse_load_test_arguments(int &argc, char **argv, std::string &error_message);
void add_load_test_runtime_context();

std::unique_ptr<client_connection> connect_client(std::uint16_t port);
void close_connection(client_connection &client);

void run_load_test_benchmark(benchmark::State &state, std::uint16_t port, std::string_view request_payload);
void run_load_test_benchmark(benchmark::State &state, std::uint16_t port, std::string_view request_payload,
                             const load_test_options &options);
void run_round_trip_benchmark(benchmark::State &state, client_connection &client, std::string_view request_payload);

} // namespace warp::benchmarks
