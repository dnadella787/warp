#include "http_event_loop_benchmark_support.hpp"

#include "warp/http/server_builder.hpp"
#include "warp/warp.hpp"

#include <string>

namespace warp::benchmarks {

namespace {

constexpr std::string_view request_payload = "GET /ping HTTP/1.1\r\n"
                                             "Host: 127.0.0.1\r\n"
                                             "Connection: keep-alive\r\n"
                                             "\r\n";

const char *mode_benchmark_name(warp::event_loop_mode mode) {
	switch (mode) {
	case warp::event_loop_mode::callbacks:
		return "BM_CallbackEventLoop_RoundTrip";
	case warp::event_loop_mode::coroutines:
		return "BM_CoroutineEventLoop_RoundTrip";
	}
	return "BM_UnknownEventLoop_RoundTrip";
}

template <warp::event_loop_mode Mode>
void register_round_trip_case(std::size_t concurrency) {
	const auto options = load_test_options_for_concurrency(concurrency);
	const auto name = std::string(mode_benchmark_name(Mode)) + "/concurrency:" + std::to_string(concurrency);
	benchmark::RegisterBenchmark(
	    name,
	    [options](benchmark::State &state) {
		    try {
			    server_fixture server(warp::http::server_builder().get("/ping",
			                                                           [](const warp::request &) -> warp::response {
				                                                           return warp::response::ok(R"({"ok":true})");
			                                                           }),
			                          event_loop_mode_tag<Mode> {});
			    state.SetLabel(format_load_test_configuration(options.client_threads));
			    run_load_test_benchmark(state, server.port, request_payload, options);
		    } catch (const std::exception &exception) {
			    state.SkipWithError(exception.what());
		    }
	    })
	    ->Iterations(1)
	    ->UseManualTime()
	    ->Unit(benchmark::kMillisecond);
}

} // namespace

void register_round_trip_benchmarks() {
	for (const auto concurrency : benchmark_concurrency_levels()) {
		register_round_trip_case<warp::event_loop_mode::callbacks>(concurrency);
		register_round_trip_case<warp::event_loop_mode::coroutines>(concurrency);
	}
}

} // namespace warp::benchmarks
