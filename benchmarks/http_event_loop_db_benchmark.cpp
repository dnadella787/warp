#include "http_event_loop_benchmark_support.hpp"

#include "warp/db/postgres/connection_pool.hpp"
#include "warp/http/body_builder.hpp"
#include "warp/http/server_builder.hpp"
#include "warp/warp.hpp"

#include <boost/asio/system_executor.hpp>

#include <string>

namespace warp::benchmarks {

namespace asio = boost::asio;

namespace {

constexpr std::string_view request_payload = "GET /db/exchanges/nyse HTTP/1.1\r\n"
                                             "Host: 127.0.0.1\r\n"
                                             "Connection: keep-alive\r\n"
                                             "\r\n";

const char *mode_benchmark_name(warp::event_loop_mode mode) {
	switch (mode) {
	case warp::event_loop_mode::callbacks:
		return "BM_CallbackEventLoop_DbRoundTrip";
	case warp::event_loop_mode::coroutines:
		return "BM_CoroutineEventLoop_DbRoundTrip";
	}
	return "BM_UnknownEventLoop_DbRoundTrip";
}

void register_db_round_trip_case(warp::event_loop_mode mode, std::size_t concurrency) {
	const auto options = load_test_options_for_concurrency(concurrency);
	const auto name = std::string(mode_benchmark_name(mode)) + "/concurrency:" + std::to_string(concurrency);
	benchmark::RegisterBenchmark(
	    name,
	    [mode, options](benchmark::State &state) {
		    auto env = load_db_env();
		    if (!env) {
			    state.SkipWithError("WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME must be set for DB benchmark");
			    return;
		    }

		    try {
			    auto db_pool = std::make_shared<warp::db::postgres::connection_pool>(asio::system_executor {},
			                                                                         make_db_config(*env), 4, 2);
			    server_fixture server(warp::http::server_builder().event_loop(mode).get(
			        "/db/exchanges/nyse", [db_pool](warp::request) -> warp::awaitable<warp::response> {
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
			    state.SetLabel(format_load_test_configuration(options.client_threads));
			    run_load_test_benchmark(state, server.port, request_payload, options);
			    db_pool->close();
		    } catch (const std::exception &exception) {
			    state.SkipWithError(exception.what());
		    }
	    })
	    ->Iterations(1)
	    ->UseManualTime()
	    ->Unit(benchmark::kMillisecond);
}

} // namespace

void register_db_round_trip_benchmarks() {
	for (const auto concurrency : benchmark_concurrency_levels()) {
		register_db_round_trip_case(warp::event_loop_mode::callbacks, concurrency);
		register_db_round_trip_case(warp::event_loop_mode::coroutines, concurrency);
	}
}

} // namespace warp::benchmarks
