#include "http_event_loop_benchmark_support.hpp"

#include "warp/server/server_builder.hpp"
#include "warp/warp.hpp"

#include <string>

namespace warp::benchmarks {

namespace {

constexpr std::string_view request_payload = "GET /ping HTTP/1.1\r\n"
                                             "Host: 127.0.0.1\r\n"
                                             "Connection: keep-alive\r\n"
                                             "\r\n";

const char *mode_benchmark_name(event_loop_mode mode) {
	switch (mode) {
	case event_loop_mode::callbacks:
		return "BM_CallbackEventLoop_RoundTrip";
	case event_loop_mode::coroutines:
		return "BM_CoroutineEventLoop_RoundTrip";
	}
	return "BM_UnknownEventLoop_RoundTrip";
}

const char *transport_benchmark_suffix(benchmark_transport transport) {
	switch (transport) {
	case benchmark_transport::plain_http:
		return "/tls:off";
	case benchmark_transport::tls:
		return "/tls:on";
	}
	return "/tls:unknown";
}

template <event_loop_mode Mode>
void register_round_trip_case(std::size_t concurrency, benchmark_transport transport) {
	const auto options = load_test_options_for_concurrency(concurrency);
	auto case_options = options;
	case_options.transport = transport;
	const auto name = std::string(mode_benchmark_name(Mode)) + "/concurrency:" + std::to_string(concurrency) +
	                  transport_benchmark_suffix(transport);
	benchmark::RegisterBenchmark(name,
	                             [case_options, transport](benchmark::State &state) {
		                             try {
			                             auto builder =
			                                 server::server_builder().get<"/ping">([](const request &) -> response {
				                                 return response::ok(R"({"ok":true})");
			                                 });
			                             if (transport == benchmark_transport::tls) {
				                             builder.ssl_config(make_benchmark_tls_server_ssl_config());
			                             }
			                             server_fixture server(std::move(builder), event_loop_mode_tag<Mode> {});
			                             state.SetLabel(format_load_test_configuration(case_options.client_threads));
			                             run_load_test_benchmark(state, server.port, request_payload, case_options);
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
		register_round_trip_case<event_loop_mode::callbacks>(concurrency, benchmark_transport::plain_http);
		register_round_trip_case<event_loop_mode::callbacks>(concurrency, benchmark_transport::tls);
		register_round_trip_case<event_loop_mode::coroutines>(concurrency, benchmark_transport::plain_http);
		register_round_trip_case<event_loop_mode::coroutines>(concurrency, benchmark_transport::tls);
	}
}

} // namespace warp::benchmarks
