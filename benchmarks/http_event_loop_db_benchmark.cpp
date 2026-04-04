#include "http_event_loop_benchmark_support.hpp"

#include "warp/db/postgres/connection_pool.hpp"
#include "warp/http/body_builder.hpp"

#include <boost/asio/system_executor.hpp>
#include "warp/warp.hpp"
#include "warp/http/server_builder.hpp"

namespace {

namespace asio = boost::asio;

class db_event_loop_benchmark : public benchmark::Fixture {
public:
	void SetUp(const benchmark::State &state) override {
		skip_reason_.clear();
		auto env = warp::benchmarks::load_db_env();
		if (!env) {
			skip_reason_ = "WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME must be set for DB benchmark";
			return;
		}

		db_pool_ = std::make_shared<warp::db::postgres::connection_pool>(asio::system_executor {},
		                                                                 warp::benchmarks::make_db_config(*env), 4, 2);

		const auto mode = static_cast<warp::event_loop_mode>(state.range(0));
		server_ = std::make_unique<warp::benchmarks::server_fixture>(warp::http::server_builder().event_loop(mode).get(
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
		client_ = warp::benchmarks::connect_client(server_->port);
	}

	void TearDown(const benchmark::State &) override {
		if (client_) {
			warp::benchmarks::close_connection(*client_);
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
	std::unique_ptr<warp::benchmarks::server_fixture> server_;
	std::unique_ptr<warp::benchmarks::client_connection> client_;
};

BENCHMARK_DEFINE_F(db_event_loop_benchmark, db_round_trip)(benchmark::State &state) {
	if (!skip_reason_.empty()) {
		state.SkipWithError(skip_reason_.c_str());
		return;
	}

	if (!client_) {
		return;
	}

	warp::benchmarks::run_round_trip_benchmark(state, *client_, request_payload);
}

BENCHMARK_REGISTER_F(db_event_loop_benchmark, db_round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::callbacks))
    ->Name("BM_CallbackEventLoop_DbRoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(warp::benchmarks::benchmark_min_time_seconds);

BENCHMARK_REGISTER_F(db_event_loop_benchmark, db_round_trip)
    ->Arg(static_cast<int>(warp::event_loop_mode::coroutines))
    ->Name("BM_CoroutineEventLoop_DbRoundTrip")
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(warp::benchmarks::benchmark_min_time_seconds);

} // namespace
