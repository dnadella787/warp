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

template <typename ModeTag>
class HttpPipeliningIntegrationTest : public ::testing::Test {};

using EventLoopModes = ::testing::Types<support::event_loop_mode_tag<event_loop_mode::callbacks>,
                                        support::event_loop_mode_tag<event_loop_mode::coroutines>>;

struct EventLoopModeNames {
	template <typename ModeTag>
	static std::string GetName(int) {
		return support::event_loop_mode_name(ModeTag::value);
	}
};

TYPED_TEST_SUITE(HttpPipeliningIntegrationTest, EventLoopModes, EventLoopModeNames);

TYPED_TEST(HttpPipeliningIntegrationTest, SlowThenFastPipelinedRequestsPreserveWireOrder) {
	auto slow_started = std::make_shared<std::atomic<bool>>(false);
	auto fast_finished = std::make_shared<std::atomic<bool>>(false);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .get("/slow",
	             [slow_started, fast_finished](request) -> awaitable<response> {
		             slow_started->store(true, std::memory_order_release);
		             co_return co_await support::delayed_ok_response(150ms, [fast_finished]() {
			             return body_builder()
			                 .set("route", "slow")
			                 .set("fast_finished_before_return", fast_finished->load(std::memory_order_acquire))
			                 .build();
		             });
	             })
	        .get("/fast",
	             [slow_started, fast_finished](const request &) -> response {
		             fast_finished->store(true, std::memory_order_release);
		             return response::ok(body_builder()
		                                     .set("route", "fast")
		                                     .set("saw_slow_started", slow_started->load(std::memory_order_acquire))
		                                     .build());
	             }),
	    TypeParam {});

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

TYPED_TEST(HttpPipeliningIntegrationTest, TenPipelinedRequestsReturnInArrivalOrder) {
	support::server_fixture fixture(
	    warp::http::server_builder().get(
	        "/echo/{id}",
	        [](const request &req) -> response {
		        return response::ok(body_builder().set("id", std::string(req.path_param("id").value_or(""))).build());
	        }),
	    TypeParam {});

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

TYPED_TEST(HttpPipeliningIntegrationTest, SlowThirdResponseDoesNotAllowLaterFastWritesToPassIt) {
	auto fast_after_three = std::make_shared<std::atomic<int>>(0);

	support::server_fixture fixture(
	    warp::http::server_builder().get(
	        "/item/{id}",
	        [fast_after_three](request req) -> awaitable<response> {
		        const auto id = std::string(req.path_param("id").value_or(""));
		        if (id == "3") {
			        // Response #3 sleeps, but still must be emitted before #4-#8.
			        co_return co_await support::delayed_ok_response(150ms, [fast_after_three, id]() {
				        return body_builder()
				            .set("id", id)
				            .set("later_fast_finished", fast_after_three->load(std::memory_order_acquire) >= 5)
				            .build();
			        });
		        }

		        if (std::stoi(id) > 3) {
			        fast_after_three->fetch_add(1, std::memory_order_acq_rel);
		        }

		        co_return response::ok(body_builder().set("id", id).build());
	        }),
	    TypeParam {});

	std::string payload;
	for (int i = 1; i <= 8; ++i)
		payload += support::make_get_request("/item/" + std::to_string(i), i == 8 ? "close" : "keep-alive");

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

TYPED_TEST(HttpPipeliningIntegrationTest, PipelineLimitReachedReadContinuesAfterPause) {
	support::server_fixture fixture(
	    warp::http::server_builder()
	        .get("/slow/{id}",
	             [](request req) -> awaitable<response> {
		             co_return co_await support::delayed_ok_response(150ms, [id = req.path_param("id").value_or("")]() {
			             return body_builder().set("id", std::string(id)).build();
		             });
	             })
	        .get("/fast/{id}",
	             [](const request &req) -> response {
		             return response::ok(
		                 body_builder().set("id", std::string(req.path_param("id").value_or(""))).build());
	             }),
	    TypeParam {});

	std::string payload;
	for (int i = 0; i < 8; ++i) {
		payload += support::make_get_request("/slow/" + std::to_string(i), "keep-alive");
	}
	for (int i = 8; i < 11; ++i) {
		payload += support::make_get_request("/fast/" + std::to_string(i), i == 10 ? "close" : "keep-alive");
	}

	auto client = support::connect_client(fixture.port);
	support::send_requests(*client, payload);

	for (int i = 0; i < 11; ++i) {
		const auto response = support::read_response(*client);
		const auto body = support::parse_object_body(response);
		EXPECT_EQ(response.result(), http::status::ok);
		EXPECT_EQ(std::string(body.at("id").as_string()), std::to_string(i));
	}
	EXPECT_TRUE(support::read_until_eof(*client));
}

} // namespace warp::tests
