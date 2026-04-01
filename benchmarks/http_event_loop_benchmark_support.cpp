#include "http_event_loop_benchmark_support.hpp"

#include <boost/asio/write.hpp>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <sys/resource.h>
#include <thread>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace warp::benchmarks {

using namespace std::chrono_literals;

namespace {

double timeval_to_seconds(const timeval &value) {
	return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_usec) / 1'000'000.0;
}

std::optional<double> process_cpu_seconds() {
	rusage usage {};
	if (::getrusage(RUSAGE_SELF, &usage) != 0) {
		return std::nullopt;
	}

	return timeval_to_seconds(usage.ru_utime) + timeval_to_seconds(usage.ru_stime);
}

std::optional<std::uint64_t> current_resident_memory_bytes() {
#if defined(__APPLE__)
	mach_task_basic_info_data_t info {};
	mach_msg_type_number_t info_count = MACH_TASK_BASIC_INFO_COUNT;
	if (::task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &info_count) !=
	    KERN_SUCCESS) {
		return std::nullopt;
	}

	return static_cast<std::uint64_t>(info.resident_size);
#elif defined(__linux__)
	std::ifstream statm("/proc/self/statm");
	std::uint64_t total_pages = 0;
	std::uint64_t resident_pages = 0;
	if (!(statm >> total_pages >> resident_pages)) {
		return std::nullopt;
	}

	const long page_size = ::sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		return std::nullopt;
	}

	return resident_pages * static_cast<std::uint64_t>(page_size);
#else
	return std::nullopt;
#endif
}

double bytes_to_mib(std::uint64_t bytes) {
	return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

} // namespace

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

void process_resource_tracker::start() {
	wall_start_ = std::chrono::steady_clock::now();
	cpu_start_seconds_ = process_cpu_seconds();
	sample_memory();
}

void process_resource_tracker::sample_memory() {
	if (const auto rss_bytes = current_resident_memory_bytes()) {
		peak_rss_bytes_ = peak_rss_bytes_ ? std::max(*peak_rss_bytes_, *rss_bytes) : *rss_bytes;
	}
}

void process_resource_tracker::record(benchmark::State &state) {
	sample_memory();

	if (const auto cpu_end_seconds = process_cpu_seconds(); cpu_start_seconds_ && cpu_end_seconds) {
		const auto cpu_seconds = std::max(0.0, *cpu_end_seconds - *cpu_start_seconds_);
		state.counters["proc_cpu_us_per_req"] =
		    benchmark::Counter(cpu_seconds * 1'000'000.0, benchmark::Counter::kAvgIterations);

		const auto wall_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start_).count();
		if (wall_seconds > 0.0) {
			state.counters["proc_cpu_pct"] = (cpu_seconds / wall_seconds) * 100.0;
		}
	}

	if (peak_rss_bytes_) {
		state.counters["rss_peak_mib"] = bytes_to_mib(*peak_rss_bytes_);
	}
}

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
			throw std::runtime_error("timed out connecting to benchmark server");
		}
		std::this_thread::sleep_for(20ms);
	}
}

void send_request(client_connection &client, std::string_view payload) {
	client.stream.expires_after(5s);
	asio::write(client.stream.socket(), asio::buffer(payload));
}

http::response<http::string_body> read_response(client_connection &client) {
	http::response<http::string_body> response;
	client.stream.expires_after(5s);
	http::read(client.stream, client.buffer, response);
	return response;
}

void close_connection(client_connection &client) {
	send_request(client, "GET /ping HTTP/1.1\r\n"
	                     "Host: 127.0.0.1\r\n"
	                     "Connection: close\r\n"
	                     "\r\n");
	auto response = read_response(client);
	benchmark::DoNotOptimize(response.result_int());
}

void run_round_trip_benchmark(benchmark::State &state, client_connection &client, std::string_view request_payload) {
	process_resource_tracker resources;
	resources.start();

	std::uint64_t completed_iterations = 0;
	for (auto _ : state) {
		send_request(client, request_payload);
		auto response = read_response(client);
		benchmark::DoNotOptimize(response.result_int());
		benchmark::DoNotOptimize(response.body());

		++completed_iterations;
		// Sample RSS occasionally so the memory counters do not meaningfully perturb the request timings.
		if ((completed_iterations % rss_sample_interval) == 0) {
			resources.sample_memory();
		}
	}

	resources.record(state);
	state.SetItemsProcessed(state.iterations());
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
	                        static_cast<std::int64_t>(request_payload.size()));
}

} // namespace warp::benchmarks
