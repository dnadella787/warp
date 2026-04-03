#include "support/integration/http_integration_harness.hpp"
#include "support/integration/http_db_test_support.hpp"

#include "warp/db/postgres/connection_pool.hpp"

#include <gtest/gtest.h>

#include <boost/asio/system_executor.hpp>

#include <memory>
#include <string>

namespace warp::tests {

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace support = warp::tests::integration_support;

class HttpDbIntegrationTest : public ::testing::TestWithParam<warp::event_loop_mode> {};

TEST_P(HttpDbIntegrationTest, DbRouteReturnsRequestedIdAndDatabaseNameWhenEnvironmentIsConfigured) {
	const auto env = support::load_db_env();
	if (!env) {
		GTEST_SKIP() << "Skipping DB integration test: WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME not set";
	}

	if (const auto probe_error = support::probe_db_connection(*env)) {
		GTEST_SKIP() << "Skipping DB integration test: " << *probe_error;
	}

	auto db_pool = std::make_shared<warp::db::postgres::connection_pool>(asio::system_executor {},
	                                                                     support::make_db_config(*env), 4, 2);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/db/{id}", [db_pool](warp::request req) -> warp::awaitable<warp::response> {
		        const auto id = std::string(req.path_param("id").value_or("0"));
		        const auto result = co_await db_pool->query(
		            std::string("select ") + id + "::int as requested_id, current_database() as database_name");
		        co_return warp::response::ok(
		            warp::body_builder()
		                .set("requested_id", result.rows() > 0 ? std::string(result.value(0, 0)) : id)
		                .set("database_name", result.rows() > 0 ? std::string(result.value(0, 1)) : std::string {})
		                .build());
	        }));

	auto client = support::connect_client(fixture.port);
	support::send_requests(*client, support::make_get_request("/db/7", "close"));

	const auto response = support::read_response(*client);
	const auto body = support::parse_object_body(response);
	EXPECT_EQ(response.result(), http::status::ok);
	EXPECT_EQ(std::string(body.at("requested_id").as_string()), "7");
	EXPECT_FALSE(body.at("database_name").as_string().empty());
	EXPECT_TRUE(support::read_until_eof(*client));

	db_pool->close();
}

INSTANTIATE_TEST_SUITE_P(EventLoopModes, HttpDbIntegrationTest,
                         ::testing::Values(warp::event_loop_mode::callbacks, warp::event_loop_mode::coroutines),
                         [](const ::testing::TestParamInfo<warp::event_loop_mode> &info) {
	                         return support::event_loop_mode_name(info.param);
                         });

} // namespace warp::tests
