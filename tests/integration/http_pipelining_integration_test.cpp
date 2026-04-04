#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>

#include "warp/http/server_builder.hpp"

namespace warp::tests {

namespace http = boost::beast::http;
namespace support = integration_support;
using namespace std::chrono_literals;

class HttpPipeliningIntegrationTest : public ::testing::TestWithParam<warp::event_loop_mode> {};

TEST_P(HttpPipeliningIntegrationTest, SlowThenFastPipelinedRequestsPreserveWireOrder) {
	auto slow_started = std::make_shared<std::atomic<bool>>(false);
	auto fast_finished = std::make_shared<std::atomic<bool>>(false);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/slow",
	             [slow_started, fast_finished](warp::request) -> warp::awaitable<warp::response> {
		             slow_started->store(true, std::memory_order_release);
		             co_return co_await support::delayed_ok_response(150ms, [fast_finished]() {
			             return warp::body_builder()
			                 .set("route", "slow")
			                 .set("fast_finished_before_return", fast_finished->load(std::memory_order_acquire))
			                 .build();
		             });
	             })
	        .get("/fast", [slow_started, fast_finished](const warp::request &) -> warp::response {
		        fast_finished->store(true, std::memory_order_release);
		        return warp::response::ok(warp::body_builder()
		                                      .set("route", "fast")
		                                      .set("saw_slow_started", slow_started->load(std::memory_order_acquire))
		                                      .build());
	        }));

	auto client = support::connect_client(fixture.port);
	const auto payload = support::make_get_request("/slow") + support::make_get_request("/fast", "close");
	support::send_requests(*client, payload);

	const auto slow = support::read_response(*client);
	const auto fast = support::read_response(*client);
	const auto slow_body = support::parse_object_body(slow);
	const auto fast_body = support::parse_object_body(fast);

	EXPECT_EQ(slow.result(), http::status::ok);
	EXPECT_EQ(fast.result(), http::status::ok);
	EXPECT_EQ(std::string(slow_body.at("route").as_string()), "slow");
	EXPECT_TRUE(slow_body.at("fast_finished_before_return").as_bool());
	EXPECT_EQ(std::string(fast_body.at("route").as_string()), "fast");
	EXPECT_TRUE(fast_body.at("saw_slow_started").as_bool());
	EXPECT_TRUE(support::read_until_eof(*client));
}

TEST_P(HttpPipeliningIntegrationTest, TenPipelinedRequestsReturnInArrivalOrder) {
	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/echo/{id}", [](const warp::request &req) -> warp::response {
		        return warp::response::ok(
		            warp::body_builder().set("id", std::string(req.path_param("id").value_or(""))).build());
	        }));

	std::string payload;
	for (int i = 0; i < 10; ++i) {
		payload += support::make_get_request("/echo/" + std::to_string(i), i == 9 ? "close" : "keep-alive");
	}

	auto client = support::connect_client(fixture.port);
	support::send_requests(*client, payload);

	for (int i = 0; i < 10; ++i) {
		const auto response = support::read_response(*client);
		const auto body = support::parse_object_body(response);
		EXPECT_EQ(response.result(), http::status::ok);
		EXPECT_EQ(std::string(body.at("id").as_string()), std::to_string(i));
	}
	EXPECT_TRUE(support::read_until_eof(*client));
}

TEST_P(HttpPipeliningIntegrationTest, SlowThirdResponseDoesNotAllowLaterFastWritesToPassIt) {
	auto fast_after_three = std::make_shared<std::atomic<int>>(0);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/item/{id}", [fast_after_three](warp::request req) -> warp::awaitable<warp::response> {
		        const auto id = std::string(req.path_param("id").value_or(""));
		        if (id == "3") {
			        // Response #3 sleeps, but still must be emitted before #4-#8.
			        co_return co_await support::delayed_ok_response(150ms, [fast_after_three, id]() {
				        return warp::body_builder()
				            .set("id", id)
				            .set("later_fast_finished", fast_after_three->load(std::memory_order_acquire) >= 5)
				            .build();
			        });
		        }

		        if (std::stoi(id) > 3) {
			        fast_after_three->fetch_add(1, std::memory_order_acq_rel);
		        }

		        co_return warp::response::ok(warp::body_builder().set("id", id).build());
	        }));

	std::string payload;
	for (int i = 1; i <= 8; ++i) {
		payload += support::make_get_request("/item/" + std::to_string(i), i == 8 ? "close" : "keep-alive");
	}

	auto client = support::connect_client(fixture.port);
	support::send_requests(*client, payload);

	for (int i = 1; i <= 8; ++i) {
		const auto response = support::read_response(*client);
		const auto body = support::parse_object_body(response);
		EXPECT_EQ(response.result(), http::status::ok);
		EXPECT_EQ(std::string(body.at("id").as_string()), std::to_string(i));
		if (i == 3) {
			EXPECT_TRUE(body.at("later_fast_finished").as_bool());
		}
	}
	EXPECT_TRUE(support::read_until_eof(*client));
}

INSTANTIATE_TEST_SUITE_P(EventLoopModes, HttpPipeliningIntegrationTest,
                         ::testing::Values(warp::event_loop_mode::callbacks, warp::event_loop_mode::coroutines),
                         [](const ::testing::TestParamInfo<warp::event_loop_mode> &info) {
	                         return support::event_loop_mode_name(info.param);
                         });

} // namespace warp::tests
