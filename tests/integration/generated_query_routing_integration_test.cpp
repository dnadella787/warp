#include "generated_query_routing_api_resources.hpp"
#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "warp/warp.hpp"

namespace warp::tests {

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace generated = generated_query_routing_api;
namespace support = integration_support;
using namespace std::chrono_literals;

struct query_route_observations {
	std::atomic<bool> slow_started {false};
	std::atomic<bool> fast_finished {false};
};

class generated_reports_resource {
public:
	explicit generated_reports_resource(std::shared_ptr<query_route_observations> observations)
	    : observations_(std::move(observations)) {
	}

	generated::reports_fetch_report_response fetch_report(generated::reports_fetch_report_request request) {
		observations_->fast_finished.store(true, std::memory_order_release);

		generated::reports_fetch_report_response response;
		response.body.route = "full";
		response.body.report_id = request.report_id;
		response.body.saw_slow_started = observations_->slow_started.load(std::memory_order_acquire);
		return response;
	}

	generated::reports_fetch_report_summary_response
	fetch_report_summary(generated::reports_fetch_report_summary_request request) {
		generated::reports_fetch_report_summary_response response;
		response.body.route = "summary";
		response.body.report_id = request.report_id;
		response.body.summary = request.summary;
		return response;
	}

	generated::reports_fetch_report_projection_response
	fetch_report_projection(generated::reports_fetch_report_projection_request request) {
		generated::reports_fetch_report_projection_response response;
		response.body.route = "projection";
		response.body.report_id = request.report_id;
		response.body.fields = request.fields;
		return response;
	}

	warp::awaitable<generated::reports_fetch_report_summary_projection_response>
	fetch_report_summary_projection(generated::reports_fetch_report_summary_projection_request request) {
		if (request.report_id == "slow") {
			observations_->slow_started.store(true, std::memory_order_release);
			const auto executor = co_await asio::this_coro::executor;
			asio::steady_timer timer(executor);
			timer.expires_after(150ms);
			co_await timer.async_wait(asio::use_awaitable);
		}

		generated::reports_fetch_report_summary_projection_response response;
		response.body.route = "summary_projection";
		response.body.report_id = request.report_id;
		response.body.summary = request.summary;
		response.body.fields = request.fields;
		response.body.fast_finished_before_return = observations_->fast_finished.load(std::memory_order_acquire);
		co_return response;
	}

private:
	std::shared_ptr<query_route_observations> observations_;
};

template <typename ModeTag>
class GeneratedQueryRoutingIntegrationTest : public ::testing::Test {};

using EventLoopModes = ::testing::Types<support::event_loop_mode_tag<event_loop_mode::callbacks>,
                                        support::event_loop_mode_tag<event_loop_mode::coroutines>>;

struct EventLoopModeNames {
	template <typename ModeTag>
	static std::string GetName(int) {
		return support::event_loop_mode_name(ModeTag::value);
	}
};

TYPED_TEST_SUITE(GeneratedQueryRoutingIntegrationTest, EventLoopModes, EventLoopModeNames);

TYPED_TEST(GeneratedQueryRoutingIntegrationTest, ResolvesOverlappingQueryRoutesBySpecificityAndFallback) {
	auto observations = std::make_shared<query_route_observations>();
	auto service = std::make_shared<generated_reports_resource>(observations);
	generated::reports_api_routes<generated_reports_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes), TypeParam {});
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string payload = support::make_get_request("/reports/42?summary=true&fields=name") +
	                            support::make_get_request("/reports/42?summary=true") +
	                            support::make_get_request("/reports/42?fields=name") +
	                            support::make_get_request("/reports/42?unused=1", "close");
	support::send_requests(*client, payload);

	const auto summary_projection = support::read_response(*client);
	const auto summary = support::read_response(*client);
	const auto projection = support::read_response(*client);
	const auto fallback = support::read_response(*client);

	const auto summary_projection_body = support::parse_object_body(summary_projection);
	const auto summary_body = support::parse_object_body(summary);
	const auto projection_body = support::parse_object_body(projection);
	const auto fallback_body = support::parse_object_body(fallback);

	EXPECT_EQ(summary_projection.result(), http::status::ok);
	EXPECT_EQ(std::string(summary_projection_body.at("route").as_string()), "summary_projection");
	EXPECT_EQ(std::string(summary_projection_body.at("report_id").as_string()), "42");
	EXPECT_TRUE(summary_projection_body.at("summary").as_bool());
	EXPECT_EQ(std::string(summary_projection_body.at("fields").as_string()), "name");

	EXPECT_EQ(summary.result(), http::status::ok);
	EXPECT_EQ(std::string(summary_body.at("route").as_string()), "summary");
	EXPECT_EQ(std::string(summary_body.at("report_id").as_string()), "42");
	EXPECT_TRUE(summary_body.at("summary").as_bool());

	EXPECT_EQ(projection.result(), http::status::ok);
	EXPECT_EQ(std::string(projection_body.at("route").as_string()), "projection");
	EXPECT_EQ(std::string(projection_body.at("report_id").as_string()), "42");
	EXPECT_EQ(std::string(projection_body.at("fields").as_string()), "name");

	EXPECT_EQ(fallback.result(), http::status::ok);
	EXPECT_EQ(std::string(fallback_body.at("route").as_string()), "full");
	EXPECT_EQ(std::string(fallback_body.at("report_id").as_string()), "42");

	EXPECT_TRUE(support::read_until_eof(*client));
}

TYPED_TEST(GeneratedQueryRoutingIntegrationTest, ReturnsBadRequestForDuplicateAndMalformedQueriesOnOverlappingRoutes) {
	auto observations = std::make_shared<query_route_observations>();
	auto service = std::make_shared<generated_reports_resource>(observations);
	generated::reports_api_routes<generated_reports_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes), TypeParam {});
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string payload = support::make_get_request("/reports/42?summary=true&summary=false") +
	                            support::make_get_request("/reports/42?fields=%ZZ", "close");
	support::send_requests(*client, payload);

	const auto duplicate = support::read_response(*client);
	const auto malformed = support::read_response(*client);
	const auto duplicate_body = support::parse_object_body(duplicate);
	const auto malformed_body = support::parse_object_body(malformed);

	EXPECT_EQ(duplicate.result(), http::status::bad_request);
	EXPECT_EQ(std::string(duplicate_body.at("error").as_string()), "duplicate query parameter 'summary'");

	EXPECT_EQ(malformed.result(), http::status::bad_request);
	EXPECT_EQ(std::string(malformed_body.at("error").as_string()),
	          "malformed percent-encoding in query parameter 'fields'");

	EXPECT_TRUE(support::read_until_eof(*client));
}

TYPED_TEST(GeneratedQueryRoutingIntegrationTest, HandlesConcurrentClientsAcrossOverlappingQueryRoutes) {
	auto observations = std::make_shared<query_route_observations>();
	auto service = std::make_shared<generated_reports_resource>(observations);
	generated::reports_api_routes<generated_reports_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes), TypeParam {});
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	http::response<http::string_body> slow_response;
	http::response<http::string_body> fast_response;

	std::thread slow_client([&] {
		auto client = support::connect_client(fixture->port);
		support::send_requests(*client, support::make_get_request("/reports/slow?summary=true&fields=name", "close"));
		slow_response = support::read_response(*client);
		EXPECT_TRUE(support::read_until_eof(*client));
	});

	while (!observations->slow_started.load(std::memory_order_acquire)) {
		std::this_thread::sleep_for(5ms);
	}

	std::thread fast_client([&] {
		auto client = support::connect_client(fixture->port);
		support::send_requests(*client, support::make_get_request("/reports/fast", "close"));
		fast_response = support::read_response(*client);
		EXPECT_TRUE(support::read_until_eof(*client));
	});

	slow_client.join();
	fast_client.join();

	const auto slow_body = support::parse_object_body(slow_response);
	const auto fast_body = support::parse_object_body(fast_response);

	EXPECT_EQ(slow_response.result(), http::status::ok);
	EXPECT_EQ(std::string(slow_body.at("route").as_string()), "summary_projection");
	EXPECT_EQ(std::string(slow_body.at("report_id").as_string()), "slow");
	EXPECT_TRUE(slow_body.at("fast_finished_before_return").as_bool());

	EXPECT_EQ(fast_response.result(), http::status::ok);
	EXPECT_EQ(std::string(fast_body.at("route").as_string()), "full");
	EXPECT_EQ(std::string(fast_body.at("report_id").as_string()), "fast");
	EXPECT_TRUE(fast_body.at("saw_slow_started").as_bool());
}

} // namespace warp::tests
