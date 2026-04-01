#include "warp/http/server.hpp"
#include "warp/db/postgres/connection_config.hpp"
#include "warp/db/postgres/connection_pool.hpp"

#include <benchmark/benchmark.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <thread>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

constexpr std::uint64_t rss_sample_interval = 128;

struct db_env {
	std::string host;
	std::optional<std::uint16_t> port;
	std::string user;
	std::string password;
	std::string database;
};

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

class process_resource_tracker {
public:
	void start() {
		wall_start_ = std::chrono::steady_clock::now();
		cpu_start_seconds_ = process_cpu_seconds();
		sample_memory();
	}

	void sample_memory() {
		if (const auto rss_bytes = current_resident_memory_bytes()) {
			peak_rss_bytes_ = peak_rss_bytes_ ? std::max(*peak_rss_bytes_, *rss_bytes) : *rss_bytes;
		}
	}

	void record(benchmark::State &state) {
		sample_memory();

		if (const auto cpu_end_seconds = process_cpu_seconds(); cpu_start_seconds_ && cpu_end_seconds) {
			const auto cpu_seconds = std::max(0.0, *cpu_end_seconds - *cpu_start_seconds_);
			state.counters["proc_cpu_us_per_req"] =
			    benchmark::Counter(cpu_seconds * 1'000'000.0, benchmark::Counter::kAvgIterations);

			const auto wall_seconds =
			    std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start_).count();
			if (wall_seconds > 0.0) {
				state.counters["proc_cpu_pct"] = (cpu_seconds / wall_seconds) * 100.0;
			}
		}

		if (peak_rss_bytes_) {
			state.counters["rss_peak_mib"] = bytes_to_mib(*peak_rss_bytes_);
		}
	}

private:
	std::chrono::steady_clock::time_point wall_start_ {};
	std::optional<double> cpu_start_seconds_;
	std::optional<std::uint64_t> peak_rss_bytes_;
};

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
};

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

class event_loop_benchmark : public benchmark::Fixture {
public:
	void SetUp(const benchmark::State &state) override {
		const auto mode = static_cast<warp::event_loop_mode>(state.range(0));
		server_ = std::make_unique<server_fixture>(warp::http::server_builder().event_loop(mode).get(
		    "/ping", [](const warp::request &) -> warp::response { return warp::response::ok(R"({"ok":true})"); }));
		client_ = connect_client(server_->port);
	}

	void TearDown(const benchmark::State &) override {
		if (client_) {
			close_connection(*client_);
			client_.reset();
		}
		server_.reset();
	}

protected:
	static constexpr std::string_view request_payload = "GET /ping HTTP/1.1\r\n"
	                                                    "Host: 127.0.0.1\r\n"
	                                                    "Connection: keep-alive\r\n"
	                                                    "\r\n";

	std::unique_ptr<server_fixture> server_;
	std::unique_ptr<client_connection> client_;
};

class db_event_loop_benchmark : public benchmark::Fixture {
public:
	void SetUp(const benchmark::State &state) override {
		skip_reason_.clear();
		auto env = load_db_env();
		if (!env) {
			skip_reason_ = "WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME must be set for DB benchmark";
			return;
		}

		db_pool_ =
		    std::make_shared<warp::db::postgres::connection_pool>(asio::system_executor {}, make_db_config(*env), 4, 2);

		const auto mode = static_cast<warp::event_loop_mode>(state.range(0));
		server_ = std::make_unique<server_fixture>(warp::http::server_builder().event_loop(mode).get(
		    "/db/exchanges/nyse", [db_pool = db_pool_](warp::request) -> warp::awaitable<warp::response> {
			    auto result = co_await db_pool->query(
			        "SELECT exchange_code, exchange_name FROM exchanges WHERE exchange_code = 'NYSE' LIMIT 1");
			    if (result.rows() == 0) {
				    co_return warp::response::not_found("No exchange with code=NYSE found");
			    }

			    co_return warp::response::ok(warp::body_builder()
			                                     .set("exchange_code", std::string(result.value(0, 0)))
			                                     .set("exchange_name", std::string(result.value(0, 1)))
			                                     .build());
		    }));
		client_ = connect_client(server_->port);
	}

	void TearDown(const benchmark::State &) override {
		if (client_) {
			close_connection(*client_);
			client_.reset();
		}
		server_.reset();
		if (db_pool_) {
			db_pool_->close();
			db_pool_.reset();
		}
	}

protected:
	static constexpr std::string_view request_payload = "GET /db/exchanges/nyse HTTP/1.1\r\n"
	                                                    "Host: 127.0.0.1\r\n"
	                                                    "Connection: keep-alive\r\n"
	                                                    "\r\n";

	std::string skip_reason_;
	std::shared_ptr<warp::db::postgres::connection_pool> db_pool_;
	std::unique_ptr<server_fixture> server_;
	std::unique_ptr<client_connection> client_;
};

BENCHMARK_DEFINE_F(event_loop_benchmark, round_trip)(benchmark::State &state) {
	run_round_trip_benchmark(state, *client_, request_payload);
}

BENCHMARK_REGISTER_F(event_loop_benchmark, round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::callbacks))
    ->Name("BM_CallbackEventLoop_RoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(60.0);

BENCHMARK_REGISTER_F(event_loop_benchmark, round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::coroutines))
    ->Name("BM_CoroutineEventLoop_RoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(60.0);

BENCHMARK_DEFINE_F(db_event_loop_benchmark, db_round_trip)(benchmark::State &state) {
	if (!skip_reason_.empty()) {
		state.SkipWithError(skip_reason_.c_str());
		return;
	}

	if (!client_) {
		return;
	}

	run_round_trip_benchmark(state, *client_, request_payload);
}

BENCHMARK_REGISTER_F(db_event_loop_benchmark, db_round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::callbacks))
    ->Name("BM_CallbackEventLoop_DbRoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(60.0);

BENCHMARK_REGISTER_F(db_event_loop_benchmark, db_round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::coroutines))
    ->Name("BM_CoroutineEventLoop_DbRoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(60.0);

} // namespace

BENCHMARK_MAIN();
