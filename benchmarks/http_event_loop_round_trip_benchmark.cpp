#include "http_event_loop_benchmark_support.hpp"
#include "warp/warp.hpp"
#include "warp/http/server_builder.hpp"

namespace {

class event_loop_benchmark : public benchmark::Fixture {
public:
	void SetUp(const benchmark::State &state) override {
		const auto mode = static_cast<warp::event_loop_mode>(state.range(0));
		server_ = std::make_unique<warp::benchmarks::server_fixture>(warp::http::server_builder().event_loop(mode).get(
		    "/ping", [](const warp::request &) -> warp::response { return warp::response::ok(R"({"ok":true})"); }));
		client_ = warp::benchmarks::connect_client(server_->port);
	}

	void TearDown(const benchmark::State &) override {
		if (client_) {
			warp::benchmarks::close_connection(*client_);
			client_.reset();
		}
		server_.reset();
	}

protected:
	static constexpr std::string_view request_payload = "GET /ping HTTP/1.1\r\n"
	                                                    "Host: 127.0.0.1\r\n"
	                                                    "Connection: keep-alive\r\n"
	                                                    "\r\n";

	std::unique_ptr<warp::benchmarks::server_fixture> server_;
	std::unique_ptr<warp::benchmarks::client_connection> client_;
};

BENCHMARK_DEFINE_F(event_loop_benchmark, round_trip)(benchmark::State &state) {
	warp::benchmarks::run_round_trip_benchmark(state, *client_, request_payload);
}

BENCHMARK_REGISTER_F(event_loop_benchmark, round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::callbacks))
    ->Name("BM_CallbackEventLoop_RoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(warp::benchmarks::benchmark_min_time_seconds);

BENCHMARK_REGISTER_F(event_loop_benchmark, round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::coroutines))
    ->Name("BM_CoroutineEventLoop_RoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(warp::benchmarks::benchmark_min_time_seconds);

} // namespace
