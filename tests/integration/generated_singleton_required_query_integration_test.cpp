#include "generated_singleton_required_query_api_resources.hpp"
#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/beast/http.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <string>

#include "warp/warp.hpp"

namespace warp::tests {

namespace http = boost::beast::http;
namespace generated = generated_singleton_required_query_api;
namespace support = integration_support;

class generated_singleton_required_query_resource {
public:
	generated::reports_search_response search(generated::reports_search_request request) {
		handler_calls_.fetch_add(1, std::memory_order_relaxed);

		generated::reports_search_response response;
		response.body.route = "search";
		response.body.query = request.query;
		return response;
	}

	[[nodiscard]] int handler_calls() const noexcept {
		return handler_calls_.load(std::memory_order_relaxed);
	}

private:
	std::atomic<int> handler_calls_ {0};
};

template <typename ModeTag>
class GeneratedSingletonRequiredQueryIntegrationTest : public ::testing::Test {};

using EventLoopModes = ::testing::Types<support::event_loop_mode_tag<event_loop_mode::callbacks>,
                                        support::event_loop_mode_tag<event_loop_mode::coroutines>>;

struct EventLoopModeNames {
	template <typename ModeTag>
	static std::string GetName(int) {
		return support::event_loop_mode_name(ModeTag::value);
	}
};

TYPED_TEST_SUITE(GeneratedSingletonRequiredQueryIntegrationTest, EventLoopModes, EventLoopModeNames);

TYPED_TEST(GeneratedSingletonRequiredQueryIntegrationTest,
           MissingRequiredQueryReturnsBadRequestInsteadOfNotFoundForSingletonEndpoint) {
	auto service = std::make_shared<generated_singleton_required_query_resource>();
	generated::reports_api_routes<generated_singleton_required_query_resource> routes(service);

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
	const std::string payload =
	    support::make_get_request("/reports/search") + support::make_get_request("/reports/search?query=warp", "close");
	support::send_requests(*client, payload);

	const auto missing = support::read_response(*client);
	const auto valid = support::read_response(*client);
	const auto missing_body = support::parse_object_body(missing);
	const auto valid_body = support::parse_object_body(valid);

	EXPECT_EQ(missing.result(), http::status::bad_request);
	EXPECT_EQ(std::string(missing_body.at("error").as_string()), "missing required query parameter 'query'");

	EXPECT_EQ(valid.result(), http::status::ok);
	EXPECT_EQ(std::string(valid_body.at("route").as_string()), "search");
	EXPECT_EQ(std::string(valid_body.at("query").as_string()), "warp");

	EXPECT_EQ(service->handler_calls(), 1);
	EXPECT_TRUE(support::read_until_eof(*client));
}

} // namespace warp::tests
