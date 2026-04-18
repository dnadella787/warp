#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>

#include "gmock/gmock-matchers.h"
#include "warp/warp.hpp"
#include "warp/http/server_builder.hpp"

namespace warp::tests {

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace support = integration_support;
using namespace std::chrono_literals;
using ::testing::Property;
using ::testing::Throws;

class HttpConnectionAndErrorIntegrationTest : public ::testing::TestWithParam<event_loop_mode> {};

TEST_P(HttpConnectionAndErrorIntegrationTest, ConnectionCloseStopsFollowingPipelinedRequests) {
	auto after_processed = std::make_shared<std::atomic<int>>(0);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/close",
	             [](const request &) -> response { return response::ok(body_builder().set("route", "close").build()); })
	        .get("/after", [after_processed](const request &) -> response {
		        after_processed->fetch_add(1, std::memory_order_acq_rel);
		        return response::ok(body_builder().set("route", "after").build());
	        }));

	auto client = support::connect_client(fixture.port);
	const auto payload = support::make_get_request("/close", "close") + support::make_get_request("/after", "close");
	support::send_requests(*client, payload);

	const auto response = support::read_response(*client);
	const auto body = support::parse_object_body(response);
	EXPECT_EQ(response.result(), http::status::ok);
	EXPECT_EQ(std::string(body.at("route").as_string()), "close");
	EXPECT_TRUE(support::read_until_eof(*client));

	std::this_thread::sleep_for(100ms);
	EXPECT_EQ(after_processed->load(std::memory_order_acquire), 0);
}

TEST_P(HttpConnectionAndErrorIntegrationTest, ThrowingHandlerReturnsErrorAndStillPreservesSubsequentResponseOrder) {
	auto throw_started = std::make_shared<std::atomic<bool>>(false);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/throw",
	             [throw_started](request) -> awaitable<response> {
		             throw_started->store(true, std::memory_order_release);
		             const auto executor = co_await asio::this_coro::executor;
		             asio::steady_timer timer(executor);
		             timer.expires_after(150ms);
		             co_await timer.async_wait(asio::use_awaitable);
		             throw std::runtime_error("boom");
	             })
	        .get("/fast", [throw_started](const request &) -> response {
		        return response::ok(body_builder()
		                                .set("route", "fast")
		                                .set("saw_throw_started", throw_started->load(std::memory_order_acquire))
		                                .build());
	        }));

	auto client = support::connect_client(fixture.port);
	const auto payload = support::make_get_request("/throw") + support::make_get_request("/fast", "close");
	support::send_requests(*client, payload);

	const auto first = support::read_response(*client);
	const auto second = support::read_response(*client);
	const auto second_body = support::parse_object_body(second);

	EXPECT_EQ(first.result(), http::status::internal_server_error);
	EXPECT_EQ(second.result(), http::status::ok);
	EXPECT_EQ(std::string(second_body.at("route").as_string()), "fast");
	EXPECT_TRUE(second_body.at("saw_throw_started").as_bool());
	EXPECT_TRUE(support::read_until_eof(*client));
}

TEST_P(HttpConnectionAndErrorIntegrationTest, MissingRouteResponseStillKeepsOrderingForLaterValidResponse) {
	auto fast_finished = std::make_shared<std::atomic<bool>>(false);

	support::server_fixture fixture(
	    warp::http::server_builder()
	        .event_loop(GetParam())
	        .get("/slow",
	             [fast_finished](request) -> awaitable<response> {
		             co_return co_await support::delayed_ok_response(150ms, [fast_finished]() {
			             return body_builder()
			                 .set("route", "slow")
			                 .set("fast_finished_before_return", fast_finished->load(std::memory_order_acquire))
			                 .build();
		             });
	             })
	        .get("/fast", [fast_finished](const request &) -> response {
		        fast_finished->store(true, std::memory_order_release);
		        return response::ok(body_builder().set("route", "fast").build());
	        }));

	auto client = support::connect_client(fixture.port);
	const auto payload = support::make_get_request("/slow") + support::make_get_request("/missing") +
	                     support::make_get_request("/fast", "close");
	support::send_requests(*client, payload);

	const auto first = support::read_response(*client);
	const auto second = support::read_response(*client);
	const auto third = support::read_response(*client);
	const auto first_body = support::parse_object_body(first);
	const auto second_body = support::parse_object_body(second);
	const auto third_body = support::parse_object_body(third);

	// The not-found middle request must not reorder around slow or fast responses.
	EXPECT_EQ(first.result(), http::status::ok);
	EXPECT_EQ(std::string(first_body.at("route").as_string()), "slow");
	EXPECT_TRUE(first_body.at("fast_finished_before_return").as_bool());
	EXPECT_EQ(second.result(), http::status::not_found);
	EXPECT_EQ(std::string(second_body.at("error").as_string()), "Not Found");
	EXPECT_EQ(third.result(), http::status::ok);
	EXPECT_EQ(std::string(third_body.at("route").as_string()), "fast");
	EXPECT_TRUE(support::read_until_eof(*client));
}

TEST_P(HttpConnectionAndErrorIntegrationTest, ConnectionClosedServerSideShouldNotContinue) {
	auto after_processed = std::make_shared<std::atomic<int>>(0);
	support::server_fixture fixture(warp::http::server_builder()
	                                    .event_loop(GetParam())
	                                    .get("/close",
	                                         [](const request &) -> response {
		                                         auto resp = response::ok(body_builder().set("route", "close").build());
		                                         resp.keep_alive(false);
		                                         return resp;
	                                         })
	                                    .get("/after", [after_processed](const request &) -> response {
		                                    after_processed->fetch_add(1, std::memory_order_acq_rel);
		                                    return response::ok(body_builder().set("route", "after").build());
	                                    }));

	auto client = support::connect_client(fixture.port);
	const auto payload = support::make_get_request("/close") + support::make_get_request("/after");
	support::send_requests(*client, payload);

	const auto response = support::read_response(*client);
	EXPECT_FALSE(response.keep_alive());
	const auto body = support::parse_object_body(response);
	EXPECT_EQ(response.result(), http::status::ok);
	EXPECT_EQ(std::string(body.at("route").as_string()), "close");
	EXPECT_TRUE(support::next_response_is_eof(*client));

	std::this_thread::sleep_for(100ms);
	EXPECT_EQ(after_processed->load(std::memory_order_acquire), 0);
}

TEST_P(HttpConnectionAndErrorIntegrationTest, AsyncCloseRequestBlocksFasterSyncRequestsAfter) {
	auto after_processed = std::make_shared<std::atomic<int>>(0);
	support::server_fixture fixture(warp::http::server_builder()
	                                    .event_loop(GetParam())
	                                    .get("/close",
	                                         [](const request &) -> awaitable<response> {
		                                         auto response = co_await support::delayed_ok_response(100ms, []() {
			                                         return body_builder().set("route", "close").build();
		                                         });
		                                         response.keep_alive(false);
		                                         co_return response;
	                                         })
	                                    .get("/after", [after_processed](const request &) -> response {
		                                    after_processed->fetch_add(1, std::memory_order_acq_rel);
		                                    return response::ok(body_builder().set("route", "after").build());
	                                    }));

	auto client = support::connect_client(fixture.port);
	const auto payload = support::make_get_request("/close") + support::make_get_request("/after");
	support::send_requests(*client, payload);

	const auto response = support::read_response(*client);
	EXPECT_FALSE(response.keep_alive());
	const auto body = support::parse_object_body(response);
	EXPECT_EQ(response.result(), http::status::ok);
	EXPECT_EQ(std::string(body.at("route").as_string()), "close");
	EXPECT_TRUE(support::next_response_is_eof(*client));

	std::this_thread::sleep_for(150ms);
	EXPECT_EQ(after_processed->load(std::memory_order_acquire), 1);
}

// Uncomment once uncaught exception handling is configurable
// TEST_P(HttpConnectionAndErrorIntegrationTest, UncaughtExceptionShouldCauseConnectionClosedServerSide) {
// 	support::server_fixture fixture(
// 		warp::http::server_builder()
// 			.event_loop(event_loop_mode::callbacks)
// 			.get("/close",
// 				 [](const request &) -> response {
// 				 	 throw std::runtime_error("UncaughtExceptionShouldCauseConnectionClosedServerSide");
// 					 return response::ok(body_builder().set("status", "good").build());
// 				 })
// 			.get("/after", [](const request &) -> response {
// 				return response::ok(body_builder().set("route", "after").build());
// 			}));
//
// 	auto client = support::connect_client(fixture.port);
// 	const auto payload = support::make_get_request("/close", "keep-alive") + support::make_get_request("/after",
// "keep-alive"); 	support::send_requests(*client, payload);
//
// 	const auto response = support::read_response(*client);
// 	EXPECT_FALSE(response.keep_alive());
// 	const auto body = support::parse_object_body(response);
// 	EXPECT_EQ(response.result(), http::status::internal_server_error);
//
// 	EXPECT_THAT(
// 		[&]() { body.at("status"); },
// 		Throws<boost::system::system_error>(
// 			Property(&boost::system::system_error::code, boost::json::error::not_found)
// 		)
// 	);
//
// 	EXPECT_THAT(
// 		[c = client.get()]() { support::read_response(*c); },
// 		Throws<boost::system::system_error>(
// 			Property(&boost::system::system_error::code, boost::asio::error::eof)
// 		)
// 	);
// }

INSTANTIATE_TEST_SUITE_P(EventLoopModes, HttpConnectionAndErrorIntegrationTest,
                         ::testing::Values(warp::event_loop_mode::callbacks, warp::event_loop_mode::coroutines),
                         [](const ::testing::TestParamInfo<warp::event_loop_mode> &info) {
	                         return support::event_loop_mode_name(info.param);
                         });

} // namespace warp::tests
