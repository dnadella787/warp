#include "http_event_loop_benchmark_support.hpp"
#include "warp/server/server_builder.hpp"
#include "warp/ssl/file_cert_loader.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/http.hpp>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace warp::benchmarks {

namespace http = beast::http;
namespace ssl = asio::ssl;
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

bool starts_with(std::string_view value, std::string_view prefix) {
	return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::optional<std::string_view> split_flag_value(std::string_view arg, std::string_view flag_name) {
	if (!starts_with(arg, flag_name)) {
		return std::nullopt;
	}
	if (arg.size() == flag_name.size()) {
		return std::string_view {};
	}
	if (arg.at(flag_name.size()) != '=') {
		return std::nullopt;
	}
	return arg.substr(flag_name.size() + 1);
}

std::size_t parse_uint64(std::string_view value, const char *flag_name) {
	if (value.empty()) {
		throw std::invalid_argument(std::string(flag_name) + " must not be empty");
	}

	std::uint64_t parsed = 0;
	const auto *begin = value.data();
	const auto *end = value.data() + value.size();
	const auto [ptr, ec] = std::from_chars(begin, end, parsed);
	if (ec != std::errc {} || ptr != end) {
		throw std::invalid_argument(std::string(flag_name) + " must be an unsigned integer");
	}
	if (parsed == 0) {
		throw std::invalid_argument(std::string(flag_name) + " must be greater than zero");
	}
	if (parsed > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
		throw std::invalid_argument(std::string(flag_name) + " is too large");
	}
	return static_cast<std::size_t>(parsed);
}

std::size_t parse_count_with_suffix(std::string_view raw_value, const char *flag_name) {
	if (raw_value.empty()) {
		throw std::invalid_argument(std::string(flag_name) + " must not be empty");
	}

	std::size_t multiplier = 1;
	if (raw_value.back() == 'k' || raw_value.back() == 'K') {
		multiplier = 1'000;
		raw_value.remove_suffix(1);
	} else if (raw_value.back() == 'm' || raw_value.back() == 'M') {
		multiplier = 1'000'000;
		raw_value.remove_suffix(1);
	}

	const auto base = parse_uint64(raw_value, flag_name);
	if (base > std::numeric_limits<std::size_t>::max() / multiplier) {
		throw std::invalid_argument(std::string(flag_name) + " is too large");
	}
	return base * multiplier;
}

std::chrono::milliseconds parse_duration_value(std::string_view raw_value, const char *flag_name) {
	if (raw_value.empty()) {
		throw std::invalid_argument(std::string(flag_name) + " must not be empty");
	}

	std::chrono::milliseconds multiplier {1000};
	if (raw_value.size() >= 2 && raw_value.substr(raw_value.size() - 2) == "ms") {
		multiplier = std::chrono::milliseconds {1};
		raw_value.remove_suffix(2);
	} else if (raw_value.back() == 's' || raw_value.back() == 'S') {
		multiplier = std::chrono::seconds {1};
		raw_value.remove_suffix(1);
	} else if (raw_value.back() == 'm' || raw_value.back() == 'M') {
		multiplier = std::chrono::minutes {1};
		raw_value.remove_suffix(1);
	}

	const auto amount = parse_uint64(raw_value, flag_name);
	const auto amount_ms = static_cast<std::uint64_t>(amount) * static_cast<std::uint64_t>(multiplier.count());
	if (amount_ms > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
		throw std::invalid_argument(std::string(flag_name) + " is too large");
	}
	return std::chrono::milliseconds {static_cast<std::int64_t>(amount_ms)};
}

std::vector<std::size_t> parse_concurrency_list(std::string_view value, const char *flag_name) {
	std::vector<std::size_t> levels;
	std::size_t start = 0;
	while (start <= value.size()) {
		const auto delimiter = value.find(',', start);
		auto token =
		    value.substr(start, delimiter == std::string_view::npos ? std::string_view::npos : delimiter - start);
		if (token.empty()) {
			throw std::invalid_argument(std::string(flag_name) + " has an empty list entry");
		}

		std::size_t step = 0;
		const auto step_pos = token.find(':');
		if (step_pos != std::string_view::npos) {
			step = parse_count_with_suffix(token.substr(step_pos + 1), flag_name);
			token = token.substr(0, step_pos);
		}

		const auto range_pos = token.find('-');
		if (range_pos == std::string_view::npos) {
			levels.push_back(parse_count_with_suffix(token, flag_name));
		} else {
			const auto range_start = parse_count_with_suffix(token.substr(0, range_pos), flag_name);
			const auto range_end = parse_count_with_suffix(token.substr(range_pos + 1), flag_name);
			if (range_start > range_end) {
				throw std::invalid_argument(std::string(flag_name) + " has a descending range");
			}
			const auto range_step = step > 0 ? step : range_start;
			for (std::size_t current = range_start; current <= range_end;) {
				levels.push_back(current);
				if (range_end - current < range_step) {
					break;
				}
				current += range_step;
			}
			if (levels.back() != range_end) {
				levels.push_back(range_end);
			}
		}

		if (delimiter == std::string_view::npos) {
			break;
		}
		start = delimiter + 1;
	}

	if (levels.empty()) {
		throw std::invalid_argument(std::string(flag_name) + " must not be empty");
	}
	return levels;
}

std::optional<std::string> getenv_string(const char *name) {
	if (const char *value = std::getenv(name); value != nullptr && *value != '\0') {
		return std::string {value};
	}
	return std::nullopt;
}

std::string benchmark_source_path(std::string_view relative_path) {
	return std::string(WARP_BENCHMARK_SOURCE_DIR) + "/" + std::string(relative_path);
}

std::string benchmark_tls_fixture_path(std::string_view filename) {
	return std::string(WARP_BENCHMARK_TLS_FIXTURE_DIR) + "/" + std::string(filename);
}

const std::string &benchmark_tls_pem_bundle() {
	static const std::string pem_bundle = [] {
		std::ifstream pem_file(benchmark_tls_fixture_path("test_ca.pem"));
		if (!pem_file) {
			throw std::runtime_error("failed to open benchmark TLS fixture CA bundle");
		}
		return std::string(std::istreambuf_iterator<char>(pem_file), std::istreambuf_iterator<char>());
	}();
	return pem_bundle;
}

load_test_options default_load_test_options() {
	load_test_options options;
	const auto hardware_threads =
	    std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency()));
	options.client_threads = std::max<std::size_t>(
	    1, std::min<std::size_t>(std::min<std::size_t>(hardware_threads, 8), options.concurrency));
	return options;
}

struct runtime_options_state {
	load_test_options options {default_load_test_options()};
	std::vector<std::size_t> concurrency_levels {default_benchmark_concurrency};
};

std::optional<std::string> &runtime_init_error() {
	static std::optional<std::string> error;
	return error;
}

runtime_options_state load_runtime_from_env() {
	runtime_options_state state;

	if (const auto value = getenv_string("WARP_BENCH_CONCURRENCY")) {
		state.concurrency_levels = parse_concurrency_list(*value, "WARP_BENCH_CONCURRENCY");
		state.options.concurrency = state.concurrency_levels.front();
	}
	if (const auto value = getenv_string("WARP_BENCH_CLIENT_THREADS")) {
		state.options.client_threads = parse_uint64(*value, "WARP_BENCH_CLIENT_THREADS");
	}
	if (const auto value = getenv_string("WARP_BENCH_WARMUP")) {
		state.options.warmup = parse_duration_value(*value, "WARP_BENCH_WARMUP");
	} else if (const auto legacy_value = getenv_string("WARP_BENCH_WARMUP_SECONDS")) {
		state.options.warmup = parse_duration_value(*legacy_value + "s", "WARP_BENCH_WARMUP_SECONDS");
	}
	if (const auto value = getenv_string("WARP_BENCH_DURATION")) {
		state.options.duration = parse_duration_value(*value, "WARP_BENCH_DURATION");
	} else if (const auto legacy_value = getenv_string("WARP_BENCH_DURATION_SECONDS")) {
		state.options.duration = parse_duration_value(*legacy_value + "s", "WARP_BENCH_DURATION_SECONDS");
	}
	if (const auto value = getenv_string("WARP_BENCH_CONNECT_TIMEOUT")) {
		state.options.connect_timeout = parse_duration_value(*value, "WARP_BENCH_CONNECT_TIMEOUT");
	} else if (const auto legacy_value = getenv_string("WARP_BENCH_CONNECT_TIMEOUT_SECONDS")) {
		state.options.connect_timeout = parse_duration_value(*legacy_value + "s", "WARP_BENCH_CONNECT_TIMEOUT_SECONDS");
	}
	if (const auto value = getenv_string("WARP_BENCH_REQUEST_TIMEOUT")) {
		state.options.request_timeout = parse_duration_value(*value, "WARP_BENCH_REQUEST_TIMEOUT");
	} else if (const auto legacy_value = getenv_string("WARP_BENCH_REQUEST_TIMEOUT_SECONDS")) {
		state.options.request_timeout = parse_duration_value(*legacy_value + "s", "WARP_BENCH_REQUEST_TIMEOUT_SECONDS");
	}

	state.options.client_threads =
	    std::max<std::size_t>(1, std::min(state.options.client_threads, state.options.concurrency));
	return state;
}

runtime_options_state &runtime_state() {
	static runtime_options_state state = [] {
		try {
			return load_runtime_from_env();
		} catch (const std::exception &exception) {
			runtime_init_error() = exception.what();
			return runtime_options_state {};
		}
	}();
	return state;
}

std::int64_t steady_now_ns() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

struct shard_metrics {
	std::uint64_t successful_requests {0};
	std::uint64_t failed_requests {0};
	std::uint64_t completed_responses {0};
	std::uint64_t connect_errors {0};
	std::uint64_t write_errors {0};
	std::uint64_t read_errors {0};
	std::uint64_t response_status_errors {0};
	std::uint64_t bytes_processed {0};
	std::uint64_t latency_samples_observed {0};
	std::vector<std::uint32_t> latency_samples_us;
};

void record_latency_sample(shard_metrics &metrics, std::uint32_t latency_us) {
	++metrics.latency_samples_observed;
	metrics.latency_samples_us.push_back(latency_us);
}

class load_test_controller {
public:
	load_test_controller(load_test_options options, std::uint16_t port, std::string request_payload)
	    : options_(std::move(options)), endpoint_(asio::ip::make_address("127.0.0.1"), port),
	      request_payload_(std::move(request_payload)), remaining_sessions_(options_.concurrency) {
	}

	[[nodiscard]] const load_test_options &options() const {
		return options_;
	}

	[[nodiscard]] const tcp::endpoint &endpoint() const {
		return endpoint_;
	}

	[[nodiscard]] const std::string &request_payload() const {
		return request_payload_;
	}

	[[nodiscard]] std::size_t request_payload_size() const {
		return request_payload_.size();
	}

	void on_connect_attempt() {
		connect_attempts_.fetch_add(1, std::memory_order_acq_rel);
	}

	void on_connect_failure(boost::system::error_code ec) {
		connect_failures_.fetch_add(1, std::memory_order_acq_rel);
		std::scoped_lock lock(last_connect_error_mutex_);
		last_connect_error_code_ = ec.value();
		last_connect_error_message_ = ec.message();
	}

	void on_connected() {
		const auto current = current_connected_.fetch_add(1, std::memory_order_acq_rel) + 1;
		update_connectivity_bounds(current);
		ready_cv_.notify_all();
	}

	void on_disconnected() {
		const auto current = current_connected_.fetch_sub(1, std::memory_order_acq_rel) - 1;
		update_connectivity_bounds(current);
		ready_cv_.notify_all();
	}

	bool wait_for_target_concurrency() {
		std::unique_lock lock(ready_mutex_);
		return ready_cv_.wait_for(lock, options_.connect_timeout, [this] {
			return stop_requested_.load(std::memory_order_acquire) ||
			       current_connected_.load(std::memory_order_acquire) >= options_.concurrency;
		});
	}

	void start_measurement(std::int64_t start_ns) {
		const auto end_ns = start_ns + std::chrono::duration_cast<std::chrono::nanoseconds>(options_.duration).count();
		measurement_start_ns_.store(start_ns, std::memory_order_release);
		measurement_end_ns_.store(end_ns, std::memory_order_release);

		const auto connected = current_connected_.load(std::memory_order_acquire);
		min_connected_.store(connected, std::memory_order_release);
		max_connected_.store(connected, std::memory_order_release);
		track_connectivity_bounds_.store(true, std::memory_order_release);
		measurement_started_.store(true, std::memory_order_release);
	}

	[[nodiscard]] std::string startup_diagnostics() const {
		std::string diagnostics =
		    "connected=" + std::to_string(current_connected_.load(std::memory_order_acquire)) + "/" +
		    std::to_string(options_.concurrency) +
		    ", connect_attempts=" + std::to_string(connect_attempts_.load(std::memory_order_acquire)) +
		    ", connect_failures=" + std::to_string(connect_failures_.load(std::memory_order_acquire));

		std::scoped_lock lock(last_connect_error_mutex_);
		if (!last_connect_error_message_.empty()) {
			diagnostics += ", last_connect_error=" + std::to_string(last_connect_error_code_) + " (" +
			               last_connect_error_message_ + ")";
		}
		return diagnostics;
	}

	[[nodiscard]] std::int64_t measurement_end_ns() const {
		return measurement_end_ns_.load(std::memory_order_acquire);
	}

	[[nodiscard]] bool measurement_started() const {
		return measurement_started_.load(std::memory_order_acquire);
	}

	[[nodiscard]] bool should_record(std::int64_t now_ns) const {
		if (!measurement_started()) {
			return false;
		}
		const auto start = measurement_start_ns_.load(std::memory_order_acquire);
		const auto end = measurement_end_ns_.load(std::memory_order_acquire);
		return now_ns >= start && now_ns < end;
	}

	[[nodiscard]] bool measurement_expired(std::int64_t now_ns) const {
		return measurement_started() && now_ns >= measurement_end_ns_.load(std::memory_order_acquire);
	}

	[[nodiscard]] bool should_stop() const {
		return stop_requested_.load(std::memory_order_acquire);
	}

	void request_stop() {
		track_connectivity_bounds_.store(false, std::memory_order_release);
		stop_requested_.store(true, std::memory_order_release);
		ready_cv_.notify_all();
		done_cv_.notify_all();
	}

	void on_session_complete() {
		if (remaining_sessions_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			done_cv_.notify_all();
		}
	}

	void wait_for_all_sessions() {
		std::unique_lock lock(done_mutex_);
		done_cv_.wait(lock, [this] { return remaining_sessions_.load(std::memory_order_acquire) == 0; });
	}

	void connectivity_bounds(std::size_t &min_connected, std::size_t &max_connected) const {
		if (!measurement_started()) {
			min_connected = 0;
			max_connected = 0;
			return;
		}
		min_connected = min_connected_.load(std::memory_order_acquire);
		max_connected = max_connected_.load(std::memory_order_acquire);
	}

private:
	void update_connectivity_bounds(std::size_t current) {
		if (!measurement_started() || !track_connectivity_bounds_.load(std::memory_order_acquire)) {
			return;
		}

		const auto now_ns = measurement_started() ? steady_now_ns() : 0;
		if (measurement_expired(now_ns)) {
			return;
		}

		auto min_connected = min_connected_.load(std::memory_order_acquire);
		while (current < min_connected &&
		       !min_connected_.compare_exchange_weak(min_connected, current, std::memory_order_acq_rel)) {
		}

		auto max_connected = max_connected_.load(std::memory_order_acquire);
		while (current > max_connected &&
		       !max_connected_.compare_exchange_weak(max_connected, current, std::memory_order_acq_rel)) {
		}
	}

	load_test_options options_;
	tcp::endpoint endpoint_;
	std::string request_payload_;

	mutable std::mutex ready_mutex_;
	mutable std::condition_variable ready_cv_;
	mutable std::mutex done_mutex_;
	mutable std::condition_variable done_cv_;

	std::atomic<std::size_t> current_connected_ {0};
	std::atomic<std::size_t> min_connected_ {0};
	std::atomic<std::size_t> max_connected_ {0};
	std::atomic<std::size_t> remaining_sessions_ {0};
	std::atomic<std::size_t> connect_attempts_ {0};
	std::atomic<std::size_t> connect_failures_ {0};
	std::atomic<bool> measurement_started_ {false};
	std::atomic<bool> track_connectivity_bounds_ {false};
	std::atomic<bool> stop_requested_ {false};
	std::atomic<std::int64_t> measurement_start_ns_ {0};
	std::atomic<std::int64_t> measurement_end_ns_ {0};
	mutable std::mutex last_connect_error_mutex_;
	int last_connect_error_code_ {0};
	std::string last_connect_error_message_;
};

template <benchmark_transport Transport>
struct benchmark_transport_traits;

template <>
struct benchmark_transport_traits<benchmark_transport::plain_http> {
	using stream_type = beast::tcp_stream;

	static stream_type make_stream(asio::io_context &ioc, ssl::context &) {
		return stream_type(ioc);
	}

	static void prepare_stream(stream_type &, ssl::context &) {
	}

	static beast::tcp_stream &lowest_layer(stream_type &stream) {
		return stream;
	}

	static asio::awaitable<void> async_handshake(stream_type &, beast::error_code &ec) {
		ec = {};
		co_return;
	}

	static void close(stream_type &stream) {
		beast::error_code ec;
		stream.socket().shutdown(tcp::socket::shutdown_both, ec);
		stream.socket().close(ec);
	}
};

template <>
struct benchmark_transport_traits<benchmark_transport::tls> {
	using stream_type = ssl::stream<beast::tcp_stream>;

	static stream_type make_stream(asio::io_context &ioc, ssl::context &ssl_ctx) {
		return stream_type(ioc, ssl_ctx);
	}

	static void prepare_stream(stream_type &stream, ssl::context &ssl_ctx) {
		stream.set_verify_mode(ssl::verify_peer);
		const auto &pem_bundle = benchmark_tls_pem_bundle();
		ssl_ctx.add_certificate_authority(asio::buffer(pem_bundle.data(), pem_bundle.size()));
	}

	static beast::tcp_stream &lowest_layer(stream_type &stream) {
		return beast::get_lowest_layer(stream);
	}

	static asio::awaitable<void> async_handshake(stream_type &stream, beast::error_code &ec) {
		co_await stream.async_handshake(ssl::stream_base::client, asio::redirect_error(asio::use_awaitable, ec));
	}

	static void close(stream_type &stream) {
		beast::error_code ec;
		auto &socket = beast::get_lowest_layer(stream);
		socket.socket().shutdown(tcp::socket::shutdown_both, ec);
		socket.socket().close(ec);
	}
};

template <benchmark_transport Transport>
class benchmark_client_session : public std::enable_shared_from_this<benchmark_client_session<Transport>> {
public:
	benchmark_client_session(asio::io_context &ioc, std::shared_ptr<load_test_controller> controller,
	                         shard_metrics &metrics)
	    : controller_(std::move(controller)), metrics_(metrics), ssl_ctx_(ssl::context::tls_client),
	      stream_(benchmark_transport_traits<Transport>::make_stream(ioc, ssl_ctx_)), retry_timer_(ioc) {
		benchmark_transport_traits<Transport>::prepare_stream(stream_, ssl_ctx_);
	}

	asio::awaitable<void> run() {
		try {
			for (;;) {
				const auto now_ns = steady_now_ns();
				if (controller_->should_stop() || controller_->measurement_expired(now_ns)) {
					break;
				}

				if (!connected_) {
					if (!(co_await connect())) {
						break;
					}
				}

				const auto request_start_ns = steady_now_ns();
				benchmark_transport_traits<Transport>::lowest_layer(stream_).expires_after(
				    controller_->options().request_timeout);
				controller_->on_connect_attempt();

				beast::error_code ec;
				co_await asio::async_write(stream_, asio::buffer(controller_->request_payload()),
				                           asio::redirect_error(asio::use_awaitable, ec));
				if (ec) {
					controller_->on_connect_failure(ec);
					record_transport_failure(steady_now_ns(), metrics_.write_errors);
					close();
					continue;
				}

				response_ = {};
				buffer_.consume(buffer_.size());

				benchmark_transport_traits<Transport>::lowest_layer(stream_).expires_after(
				    controller_->options().request_timeout);
				co_await http::async_read(stream_, buffer_, response_, asio::redirect_error(asio::use_awaitable, ec));
				const auto request_end_ns = steady_now_ns();
				if (ec) {
					record_transport_failure(request_end_ns, metrics_.read_errors);
					close();
					continue;
				}

				record_response(request_end_ns - request_start_ns, request_end_ns, response_.result_int());
				if (!response_.keep_alive()) {
					close();
				}
			}
		} catch (const std::exception &) {
			record_transport_failure(steady_now_ns(), metrics_.read_errors);
			close();
		}

		close();
		controller_->on_session_complete();
		co_return;
	}

private:
	asio::awaitable<bool> connect() {
		for (;;) {
			const auto now_ns = steady_now_ns();
			if (controller_->should_stop() || controller_->measurement_expired(now_ns)) {
				co_return false;
			}

			benchmark_transport_traits<Transport>::lowest_layer(stream_).expires_after(
			    controller_->options().connect_timeout);

			beast::error_code ec;
			co_await benchmark_transport_traits<Transport>::lowest_layer(stream_).async_connect(
			    controller_->endpoint(), asio::redirect_error(asio::use_awaitable, ec));
			if (!ec) {
				co_await benchmark_transport_traits<Transport>::async_handshake(stream_, ec);
			}
			if (!ec) {
				connected_ = true;
				controller_->on_connected();
				co_return true;
			}

			controller_->on_connect_failure(ec);
			record_transport_failure(steady_now_ns(), metrics_.connect_errors);
			close();

			retry_timer_.expires_after(20ms);
			co_await retry_timer_.async_wait(asio::redirect_error(asio::use_awaitable, ec));
		}
	}

	void record_response(std::int64_t latency_ns, std::int64_t completed_ns, int status_code) {
		if (!controller_->should_record(completed_ns)) {
			return;
		}

		++metrics_.completed_responses;
		metrics_.bytes_processed += controller_->request_payload_size();

		if (status_code >= 200 && status_code < 300) {
			++metrics_.successful_requests;
		} else {
			++metrics_.failed_requests;
			++metrics_.response_status_errors;
		}

		const auto latency_us = static_cast<std::uint32_t>(std::max<std::int64_t>(
		    1, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::nanoseconds {latency_ns}).count()));
		record_latency_sample(metrics_, latency_us);
	}

	void record_transport_failure(std::int64_t occurred_ns, std::uint64_t &error_counter) {
		if (!controller_->should_record(occurred_ns)) {
			return;
		}
		++metrics_.failed_requests;
		++error_counter;
	}

	void close() {
		if (connected_) {
			connected_ = false;
			controller_->on_disconnected();
		}

		retry_timer_.cancel();
		benchmark_transport_traits<Transport>::close(stream_);
		buffer_.consume(buffer_.size());
	}

	std::shared_ptr<load_test_controller> controller_;
	shard_metrics &metrics_;
	ssl::context ssl_ctx_;
	typename benchmark_transport_traits<Transport>::stream_type stream_;
	asio::steady_timer retry_timer_;
	beast::flat_buffer buffer_;
	http::response<http::string_body> response_;
	bool connected_ {false};
};

struct shard_runtime {
	asio::io_context ioc;
	asio::executor_work_guard<asio::io_context::executor_type> work_guard {asio::make_work_guard(ioc)};
	shard_metrics metrics;
	std::thread thread;
};

struct load_test_run_result {
	load_test_metrics metrics;
	std::uint64_t bytes_processed {0};
	std::size_t min_connected {0};
	std::size_t max_connected {0};
	double cpu_seconds {0.0};
	double wall_seconds {0.0};
	std::optional<double> peak_rss_mib;
};

load_test_options normalize_options(load_test_options options) {
	options.concurrency = std::max<std::size_t>(1, options.concurrency);
	options.client_threads = std::max<std::size_t>(1, options.client_threads);
	options.client_threads = std::min(options.client_threads, options.concurrency);
	options.warmup = std::max(options.warmup, std::chrono::milliseconds {0});
	options.duration = std::max(options.duration, std::chrono::milliseconds {1});
	options.connect_timeout = std::max(options.connect_timeout, std::chrono::milliseconds {1});
	options.request_timeout = std::max(options.request_timeout, std::chrono::milliseconds {1});
	options.sample_period = std::max(options.sample_period, std::chrono::milliseconds {1});
	return options;
}

std::optional<std::uint64_t> current_fd_soft_limit() {
	rlimit limits {};
	if (::getrlimit(RLIMIT_NOFILE, &limits) != 0) {
		return std::nullopt;
	}
	if (limits.rlim_cur == RLIM_INFINITY) {
		return std::nullopt;
	}
	return static_cast<std::uint64_t>(limits.rlim_cur);
}

double percentile_value(std::vector<std::uint32_t> &samples, double quantile) {
	if (samples.empty()) {
		return 0.0;
	}
	std::sort(samples.begin(), samples.end());
	const auto rank = static_cast<std::size_t>(std::ceil(quantile * static_cast<double>(samples.size())));
	const auto index = std::min(samples.size() - 1, rank > 0 ? rank - 1 : 0);
	return static_cast<double>(samples[index]);
}

load_test_run_result execute_load_test(std::uint16_t port, std::string_view request_payload,
                                       const load_test_options &configured_options) {
	const auto options = normalize_options(configured_options);
	auto controller = std::make_shared<load_test_controller>(options, port, std::string {request_payload});

	std::vector<std::unique_ptr<shard_runtime>> shards;
	shards.reserve(options.client_threads);
	for (std::size_t index = 0; index < options.client_threads; ++index) {
		shards.push_back(std::make_unique<shard_runtime>());
	}

	if (const auto fd_limit = current_fd_soft_limit()) {
		const auto required_fds = static_cast<std::uint64_t>(options.concurrency) + 256;
		if (required_fds > *fd_limit) {
			throw std::runtime_error(
			    "configured concurrency exceeds the current file descriptor limit; raise ulimit -n");
		}
	}

	for (std::size_t index = 0; index < options.concurrency; ++index) {
		auto &shard = *shards[index % shards.size()];
		switch (options.transport) {
		case benchmark_transport::plain_http: {
			auto session = std::make_shared<benchmark_client_session<benchmark_transport::plain_http>>(
			    shard.ioc, controller, shard.metrics);
			asio::co_spawn(
			    shard.ioc, [session]() -> asio::awaitable<void> { co_await session->run(); }, asio::detached);
			break;
		}
		case benchmark_transport::tls: {
			auto session = std::make_shared<benchmark_client_session<benchmark_transport::tls>>(shard.ioc, controller,
			                                                                                    shard.metrics);
			asio::co_spawn(
			    shard.ioc, [session]() -> asio::awaitable<void> { co_await session->run(); }, asio::detached);
			break;
		}
		}
	}

	for (auto &shard : shards) {
		shard->thread = std::thread([&ioc = shard->ioc] { ioc.run(); });
	}

	const auto shutdown_threads = [&shards] {
		for (auto &shard : shards) {
			shard->work_guard.reset();
			shard->ioc.stop();
		}
		for (auto &shard : shards) {
			if (shard->thread.joinable()) {
				shard->thread.join();
			}
		}
	};

	load_test_run_result result;
	try {
		if (!controller->wait_for_target_concurrency()) {
			throw std::runtime_error("timed out before benchmark clients reached target concurrency (" +
			                         controller->startup_diagnostics() + ")");
		}

		if (options.warmup.count() > 0) {
			std::this_thread::sleep_for(options.warmup);
		}

		process_resource_tracker resources;
		resources.start();
		controller->start_measurement(steady_now_ns());

		for (;;) {
			const auto now_ns = steady_now_ns();
			const auto end_ns = controller->measurement_end_ns();
			if (now_ns >= end_ns) {
				break;
			}

			resources.sample_memory();
			const auto remaining_ns = end_ns - now_ns;
			const auto sleep_for = std::min<std::chrono::milliseconds>(
			    options.sample_period,
			    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds {remaining_ns}));
			if (sleep_for.count() > 0) {
				std::this_thread::sleep_for(sleep_for);
			}
		}

		resources.stop();
		controller->request_stop();
		controller->wait_for_all_sessions();
		shutdown_threads();

		result.cpu_seconds = resources.cpu_seconds();
		result.wall_seconds = resources.wall_seconds();
		result.peak_rss_mib = resources.peak_rss_mib();
		controller->connectivity_bounds(result.min_connected, result.max_connected);
	} catch (...) {
		controller->request_stop();
		controller->wait_for_all_sessions();
		shutdown_threads();
		throw;
	}

	std::vector<std::uint32_t> latency_samples;
	std::size_t total_latency_samples = 0;
	for (const auto &shard : shards) {
		total_latency_samples += shard->metrics.latency_samples_us.size();
	}
	latency_samples.reserve(total_latency_samples);

	for (auto &shard : shards) {
		result.metrics.successful_requests += shard->metrics.successful_requests;
		result.metrics.failed_requests += shard->metrics.failed_requests;
		result.metrics.completed_responses += shard->metrics.completed_responses;
		result.metrics.connect_errors += shard->metrics.connect_errors;
		result.metrics.write_errors += shard->metrics.write_errors;
		result.metrics.read_errors += shard->metrics.read_errors;
		result.metrics.response_status_errors += shard->metrics.response_status_errors;
		result.metrics.latency_samples_observed += shard->metrics.latency_samples_observed;
		result.bytes_processed += shard->metrics.bytes_processed;
		latency_samples.insert(latency_samples.end(), shard->metrics.latency_samples_us.begin(),
		                       shard->metrics.latency_samples_us.end());
	}

	result.metrics.latency_samples_kept = latency_samples.size();
	result.metrics.latency_p50_us = percentile_value(latency_samples, 0.50);
	result.metrics.latency_p90_us = percentile_value(latency_samples, 0.90);
	result.metrics.latency_p99_us = percentile_value(latency_samples, 0.99);
	result.metrics.latency_max_us =
	    latency_samples.empty()
	        ? 0.0
	        : static_cast<double>(*std::max_element(latency_samples.begin(), latency_samples.end()));

	const auto measured_seconds = std::chrono::duration<double>(options.duration).count();
	const auto attempted_requests = result.metrics.successful_requests + result.metrics.failed_requests;
	result.metrics.throughput_rps =
	    measured_seconds > 0.0 ? static_cast<double>(attempted_requests) / measured_seconds : 0.0;
	result.metrics.error_rate_pct = attempted_requests > 0 ? static_cast<double>(result.metrics.failed_requests) *
	                                                             100.0 / static_cast<double>(attempted_requests)
	                                                       : 0.0;

	return result;
}

void send_request(client_connection &client, std::string_view payload, std::chrono::milliseconds timeout) {
	client.stream.expires_after(timeout);
	asio::write(client.stream.socket(), asio::buffer(payload.data(), payload.size()));
}

http::response<http::string_body> read_response(client_connection &client, std::chrono::milliseconds timeout) {
	http::response<http::string_body> response;
	client.stream.expires_after(timeout);
	http::read(client.stream, client.buffer, response);
	return response;
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
	wall_elapsed_seconds_.reset();
	cpu_end_seconds_.reset();
	sample_memory();
}

void process_resource_tracker::stop() {
	sample_memory();
	cpu_end_seconds_ = process_cpu_seconds();
	wall_elapsed_seconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start_).count();
}

void process_resource_tracker::sample_memory() {
	if (const auto rss_bytes = current_resident_memory_bytes()) {
		peak_rss_bytes_ = peak_rss_bytes_ ? std::max(*peak_rss_bytes_, *rss_bytes) : *rss_bytes;
	}
}

double process_resource_tracker::cpu_seconds() const {
	if (!cpu_start_seconds_ || !cpu_end_seconds_) {
		return 0.0;
	}
	return std::max(0.0, *cpu_end_seconds_ - *cpu_start_seconds_);
}

double process_resource_tracker::wall_seconds() const {
	if (!wall_elapsed_seconds_) {
		return 0.0;
	}
	return std::max(0.0, *wall_elapsed_seconds_);
}

std::optional<double> process_resource_tracker::peak_rss_mib() const {
	if (!peak_rss_bytes_) {
		return std::nullopt;
	}
	return bytes_to_mib(*peak_rss_bytes_);
}

void process_resource_tracker::record(benchmark::State &state, std::uint64_t requests_processed) const {
	const auto cpu = cpu_seconds();
	const auto wall = wall_seconds();
	if (requests_processed > 0 && cpu > 0.0) {
		state.counters["proc_cpu_us_per_req"] = (cpu * 1'000'000.0) / static_cast<double>(requests_processed);
	}
	if (wall > 0.0 && cpu > 0.0) {
		state.counters["proc_cpu_pct"] = (cpu / wall) * 100.0;
	}
	if (const auto rss = peak_rss_mib()) {
		state.counters["rss_peak_mib"] = *rss;
	}
}

server_fixture::server_fixture(server::server_builder builder)
    : server_fixture(std::move(builder), event_loop_mode_tag<warp::http::event_loop_mode::callbacks> {}) {
}

server_fixture::~server_fixture() {
	server.stop();
}

std::uint16_t server_fixture::reserve_port() {
	asio::io_context ioc;
	tcp::acceptor acceptor(ioc, tcp::endpoint {tcp::v4(), 0});
	return acceptor.local_endpoint().port();
}

const load_test_options &benchmark_load_test_options() {
	return runtime_state().options;
}

void set_benchmark_load_test_options(load_test_options options) {
	options = normalize_options(options);
	auto &state = runtime_state();
	state.options = options;
	state.concurrency_levels = {options.concurrency};
}

const std::vector<std::size_t> &benchmark_concurrency_levels() {
	return runtime_state().concurrency_levels;
}

load_test_options load_test_options_for_concurrency(std::size_t concurrency) {
	auto options = benchmark_load_test_options();
	options.concurrency = std::max<std::size_t>(1, concurrency);
	options.client_threads = std::max<std::size_t>(1, std::min(options.client_threads, options.concurrency));
	return options;
}

warp::ssl::ssl_config make_benchmark_tls_server_ssl_config() {
	return warp::ssl::ssl_config(true,
	                             warp::ssl::file_cert_loader(benchmark_tls_fixture_path("test_server_identity.pem")));
}

std::string format_duration(std::chrono::milliseconds duration) {
	if (duration.count() % 1000 == 0) {
		return std::to_string(duration.count() / 1000) + "s";
	}
	return std::to_string(duration.count()) + "ms";
}

std::string format_load_test_configuration(std::size_t effective_client_threads) {
	const auto &options = benchmark_load_test_options();
	std::string configured_levels;
	const auto &levels = benchmark_concurrency_levels();
	for (std::size_t index = 0; index < levels.size(); ++index) {
		if (index > 0) {
			configured_levels += ",";
		}
		configured_levels += std::to_string(levels[index]);
	}

	return "concurrency_levels=" + configured_levels + ", client_threads=" + std::to_string(effective_client_threads) +
	       ", warmup=" + format_duration(options.warmup) + ", duration=" + format_duration(options.duration) +
	       ", connect_timeout=" + format_duration(options.connect_timeout) +
	       ", request_timeout=" + format_duration(options.request_timeout);
}

bool parse_load_test_arguments(int &argc, char **argv, std::string &error_message) {
	if (const auto &init_error = runtime_init_error(); init_error) {
		error_message = *init_error;
		return false;
	}

	auto &state = runtime_state();
	std::vector<char *> passthrough;
	passthrough.reserve(static_cast<std::size_t>(argc));
	passthrough.push_back(argv[0]);

	auto parse_or_error = [&error_message](auto &&fn) -> bool {
		try {
			fn();
			return true;
		} catch (const std::exception &exception) {
			error_message = exception.what();
			return false;
		}
	};

	for (int index = 1; index < argc; ++index) {
		const std::string_view arg {argv[index]};
		bool consumed = false;

		if (const auto bench_value = split_flag_value(arg, "--warp-bench-concurrency");
		    bench_value || split_flag_value(arg, "--warp-benchmark-concurrency") ||
		    split_flag_value(arg, "--warp_benchmark_concurrency")) {
			const auto value = bench_value ? *bench_value
			                   : split_flag_value(arg, "--warp-benchmark-concurrency")
			                       ? *split_flag_value(arg, "--warp-benchmark-concurrency")
			                       : *split_flag_value(arg, "--warp_benchmark_concurrency");
			consumed = true;
			if (!parse_or_error([&] {
				    state.concurrency_levels = parse_concurrency_list(value, "--warp-benchmark-concurrency");
				    state.options.concurrency = state.concurrency_levels.front();
			    })) {
				return false;
			}
		} else if (const auto bench_value = split_flag_value(arg, "--warp-bench-client-threads");
		           bench_value || split_flag_value(arg, "--warp-benchmark-client-threads") ||
		           split_flag_value(arg, "--warp_benchmark_client_threads")) {
			const auto value = bench_value ? *bench_value
			                   : split_flag_value(arg, "--warp-benchmark-client-threads")
			                       ? *split_flag_value(arg, "--warp-benchmark-client-threads")
			                       : *split_flag_value(arg, "--warp_benchmark_client_threads");
			consumed = true;
			if (!parse_or_error(
			        [&] { state.options.client_threads = parse_uint64(value, "--warp-benchmark-client-threads"); })) {
				return false;
			}
		} else if (const auto bench_value = split_flag_value(arg, "--warp-bench-warmup");
		           bench_value || split_flag_value(arg, "--warp-benchmark-warmup") ||
		           split_flag_value(arg, "--warp_benchmark_warmup")) {
			const auto value = bench_value ? *bench_value
			                   : split_flag_value(arg, "--warp-benchmark-warmup")
			                       ? *split_flag_value(arg, "--warp-benchmark-warmup")
			                       : *split_flag_value(arg, "--warp_benchmark_warmup");
			consumed = true;
			if (!parse_or_error(
			        [&] { state.options.warmup = parse_duration_value(value, "--warp-benchmark-warmup"); })) {
				return false;
			}
		} else if (const auto bench_value = split_flag_value(arg, "--warp-bench-duration");
		           bench_value || split_flag_value(arg, "--warp-benchmark-duration") ||
		           split_flag_value(arg, "--warp_benchmark_duration")) {
			const auto value = bench_value ? *bench_value
			                   : split_flag_value(arg, "--warp-benchmark-duration")
			                       ? *split_flag_value(arg, "--warp-benchmark-duration")
			                       : *split_flag_value(arg, "--warp_benchmark_duration");
			consumed = true;
			if (!parse_or_error(
			        [&] { state.options.duration = parse_duration_value(value, "--warp-benchmark-duration"); })) {
				return false;
			}
		} else if (const auto bench_value = split_flag_value(arg, "--warp-bench-connect-timeout");
		           bench_value || split_flag_value(arg, "--warp-benchmark-connect-timeout") ||
		           split_flag_value(arg, "--warp_benchmark_connect_timeout")) {
			const auto value = bench_value ? *bench_value
			                   : split_flag_value(arg, "--warp-benchmark-connect-timeout")
			                       ? *split_flag_value(arg, "--warp-benchmark-connect-timeout")
			                       : *split_flag_value(arg, "--warp_benchmark_connect_timeout");
			consumed = true;
			if (!parse_or_error([&] {
				    state.options.connect_timeout = parse_duration_value(value, "--warp-benchmark-connect-timeout");
			    })) {
				return false;
			}
		} else if (const auto bench_value = split_flag_value(arg, "--warp-bench-request-timeout");
		           bench_value || split_flag_value(arg, "--warp-benchmark-request-timeout") ||
		           split_flag_value(arg, "--warp_benchmark_request_timeout")) {
			const auto value = bench_value ? *bench_value
			                   : split_flag_value(arg, "--warp-benchmark-request-timeout")
			                       ? *split_flag_value(arg, "--warp-benchmark-request-timeout")
			                       : *split_flag_value(arg, "--warp_benchmark_request_timeout");
			consumed = true;
			if (!parse_or_error([&] {
				    state.options.request_timeout = parse_duration_value(value, "--warp-benchmark-request-timeout");
			    })) {
				return false;
			}
		} else if (starts_with(arg, "--warp-bench-") || starts_with(arg, "--warp-benchmark-") ||
		           starts_with(arg, "--warp_benchmark_")) {
			error_message = "unrecognized warp benchmark argument: " + std::string(arg);
			return false;
		}

		if (!consumed) {
			passthrough.push_back(argv[index]);
		}
	}

	state.options.concurrency = state.concurrency_levels.front();
	state.options.client_threads =
	    std::max<std::size_t>(1, std::min(state.options.client_threads, state.options.concurrency));
	state.options = normalize_options(state.options);

	argc = static_cast<int>(passthrough.size());
	for (int index = 0; index < argc; ++index) {
		argv[index] = passthrough[static_cast<std::size_t>(index)];
	}
	return true;
}

void add_load_test_runtime_context() {
	const auto &options = benchmark_load_test_options();
	std::string levels_value;
	const auto &levels = benchmark_concurrency_levels();
	for (std::size_t index = 0; index < levels.size(); ++index) {
		if (index > 0) {
			levels_value.push_back(',');
		}
		levels_value += std::to_string(levels[index]);
	}

	benchmark::AddCustomContext("warp_benchmark_concurrency_levels", levels_value);
	benchmark::AddCustomContext("warp_benchmark_client_threads", std::to_string(options.client_threads));
	benchmark::AddCustomContext("warp_benchmark_warmup", format_duration(options.warmup));
	benchmark::AddCustomContext("warp_benchmark_duration", format_duration(options.duration));
	benchmark::AddCustomContext("warp_benchmark_connect_timeout", format_duration(options.connect_timeout));
	benchmark::AddCustomContext("warp_benchmark_request_timeout", format_duration(options.request_timeout));
}

std::unique_ptr<client_connection> connect_client(std::uint16_t port) {
	auto client = std::make_unique<client_connection>();
	const auto timeout = benchmark_load_test_options().connect_timeout;
	client->stream.expires_after(timeout);

	const auto endpoint = tcp::endpoint {asio::ip::make_address("127.0.0.1"), port};
	const auto deadline = std::chrono::steady_clock::now() + timeout;
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

void close_connection(client_connection &client) {
	const auto timeout = benchmark_load_test_options().request_timeout;
	try {
		send_request(client,
		             "GET /ping HTTP/1.1\r\n"
		             "Host: 127.0.0.1\r\n"
		             "Connection: close\r\n"
		             "\r\n",
		             timeout);
		auto response = read_response(client, timeout);
		benchmark::DoNotOptimize(response.result_int());
	} catch (const std::exception &) {
	}
}

void run_load_test_benchmark(benchmark::State &state, std::uint16_t port, std::string_view request_payload) {
	run_load_test_benchmark(state, port, request_payload, benchmark_load_test_options());
}

void run_load_test_benchmark(benchmark::State &state, std::uint16_t port, std::string_view request_payload,
                             const load_test_options &options) {
	std::uint64_t total_processed = 0;
	std::uint64_t total_bytes = 0;
	std::uint64_t total_success = 0;
	std::uint64_t total_failed = 0;
	std::uint64_t total_completed = 0;
	std::uint64_t total_connect_errors = 0;
	std::uint64_t total_write_errors = 0;
	std::uint64_t total_read_errors = 0;
	std::uint64_t total_status_errors = 0;
	std::uint64_t total_samples_observed = 0;
	std::uint64_t total_samples_kept = 0;
	std::size_t min_connected = std::numeric_limits<std::size_t>::max();
	std::size_t max_connected = 0;
	double throughput_sum = 0.0;
	double error_rate_sum = 0.0;
	double p50_sum = 0.0;
	double p90_sum = 0.0;
	double p99_sum = 0.0;
	double latency_max_sum = 0.0;
	double total_cpu_seconds = 0.0;
	double total_wall_seconds = 0.0;
	std::optional<double> peak_rss_mib;

	try {
		for (auto _ : state) {
			benchmark::DoNotOptimize(_);
			const auto run = execute_load_test(port, request_payload, options);
			const auto run_processed = run.metrics.successful_requests + run.metrics.failed_requests;
			state.SetIterationTime(run.wall_seconds);

			total_processed += run_processed;
			total_bytes += run.bytes_processed;
			total_success += run.metrics.successful_requests;
			total_failed += run.metrics.failed_requests;
			total_completed += run.metrics.completed_responses;
			total_connect_errors += run.metrics.connect_errors;
			total_write_errors += run.metrics.write_errors;
			total_read_errors += run.metrics.read_errors;
			total_status_errors += run.metrics.response_status_errors;
			total_samples_observed += run.metrics.latency_samples_observed;
			total_samples_kept += run.metrics.latency_samples_kept;
			min_connected = std::min(min_connected, run.min_connected);
			max_connected = std::max(max_connected, run.max_connected);
			throughput_sum += run.metrics.throughput_rps;
			error_rate_sum += run.metrics.error_rate_pct;
			p50_sum += run.metrics.latency_p50_us;
			p90_sum += run.metrics.latency_p90_us;
			p99_sum += run.metrics.latency_p99_us;
			latency_max_sum += run.metrics.latency_max_us;
			total_cpu_seconds += run.cpu_seconds;
			total_wall_seconds += run.wall_seconds;
			if (run.peak_rss_mib) {
				peak_rss_mib = peak_rss_mib ? std::max(*peak_rss_mib, *run.peak_rss_mib) : run.peak_rss_mib;
			}
		}
	} catch (const std::exception &exception) {
		state.SkipWithError(exception.what());
		return;
	}

	state.SetItemsProcessed(static_cast<std::int64_t>(total_processed));
	state.SetBytesProcessed(static_cast<std::int64_t>(total_bytes));

	state.counters["target_concurrency"] = static_cast<double>(options.concurrency);
	state.counters["connected_min"] =
	    min_connected == std::numeric_limits<std::size_t>::max() ? 0.0 : static_cast<double>(min_connected);
	state.counters["connected_max"] = static_cast<double>(max_connected);
	state.counters["steady_concurrency_pct"] =
	    options.concurrency > 0
	        ? (100.0 *
	           static_cast<double>(min_connected == std::numeric_limits<std::size_t>::max() ? 0 : min_connected) /
	           static_cast<double>(options.concurrency))
	        : 0.0;
	state.counters["throughput_req_per_s"] = benchmark::Counter(throughput_sum, benchmark::Counter::kAvgIterations);
	state.counters["successful_requests"] =
	    benchmark::Counter(static_cast<double>(total_success), benchmark::Counter::kAvgIterations);
	state.counters["failed_requests"] =
	    benchmark::Counter(static_cast<double>(total_failed), benchmark::Counter::kAvgIterations);
	state.counters["completed_responses"] =
	    benchmark::Counter(static_cast<double>(total_completed), benchmark::Counter::kAvgIterations);
	state.counters["error_rate_pct"] = benchmark::Counter(error_rate_sum, benchmark::Counter::kAvgIterations);
	state.counters["latency_p50_us"] = benchmark::Counter(p50_sum, benchmark::Counter::kAvgIterations);
	state.counters["latency_p90_us"] = benchmark::Counter(p90_sum, benchmark::Counter::kAvgIterations);
	state.counters["latency_p99_us"] = benchmark::Counter(p99_sum, benchmark::Counter::kAvgIterations);
	state.counters["latency_max_us"] = benchmark::Counter(latency_max_sum, benchmark::Counter::kAvgIterations);
	state.counters["connect_errors"] =
	    benchmark::Counter(static_cast<double>(total_connect_errors), benchmark::Counter::kAvgIterations);
	state.counters["write_errors"] =
	    benchmark::Counter(static_cast<double>(total_write_errors), benchmark::Counter::kAvgIterations);
	state.counters["read_errors"] =
	    benchmark::Counter(static_cast<double>(total_read_errors), benchmark::Counter::kAvgIterations);
	state.counters["response_status_errors"] =
	    benchmark::Counter(static_cast<double>(total_status_errors), benchmark::Counter::kAvgIterations);
	state.counters["latency_samples_observed"] =
	    benchmark::Counter(static_cast<double>(total_samples_observed), benchmark::Counter::kAvgIterations);
	state.counters["latency_samples_kept"] =
	    benchmark::Counter(static_cast<double>(total_samples_kept), benchmark::Counter::kAvgIterations);

	if (total_processed > 0 && total_cpu_seconds > 0.0) {
		state.counters["proc_cpu_us_per_req"] =
		    (total_cpu_seconds * 1'000'000.0) / static_cast<double>(total_processed);
	}
	if (total_wall_seconds > 0.0 && total_cpu_seconds > 0.0) {
		state.counters["proc_cpu_pct"] = (total_cpu_seconds / total_wall_seconds) * 100.0;
	}
	if (peak_rss_mib) {
		state.counters["rss_peak_mib"] = *peak_rss_mib;
	}
}

void run_round_trip_benchmark(benchmark::State &state, client_connection &client, std::string_view request_payload) {
	beast::error_code ec;
	const auto endpoint = client.stream.socket().remote_endpoint(ec);
	if (ec) {
		state.SkipWithError("benchmark client is not connected");
		return;
	}
	run_load_test_benchmark(state, endpoint.port(), request_payload, benchmark_load_test_options());
}

} // namespace warp::benchmarks
