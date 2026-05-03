#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include "warp/server/server_builder.hpp"

namespace warp::tests {

namespace http = boost::beast::http;
namespace support = integration_support;

template <typename ModeTag>
class HttpTlsIntegrationTest : public ::testing::Test {};

using EventLoopModes = ::testing::Types<support::event_loop_mode_tag<event_loop_mode::callbacks>,
                                        support::event_loop_mode_tag<event_loop_mode::coroutines>>;

struct EventLoopModeNames {
	template <typename ModeTag>
	static std::string GetName(int) {
		return support::event_loop_mode_name(ModeTag::value);
	}
};

TYPED_TEST_SUITE(HttpTlsIntegrationTest, EventLoopModes, EventLoopModeNames);

TYPED_TEST(HttpTlsIntegrationTest, TrustedTlsClientGetsResponseAndGracefulClose) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	auto client = support::connect_tls_client(fixture.port);
	support::send_requests(*client, support::make_get_request("/secure", "close"));

	const auto resp = support::read_response(*client);
	EXPECT_EQ(resp.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(resp).at("route").as_string()), "secure");
	EXPECT_TRUE(support::read_until_eof(*client));
}

TYPED_TEST(HttpTlsIntegrationTest, StalledTlsClientAfterFinalResponseTimesOutAndServerClosesConnection) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	auto stalled_client = support::connect_tls_client(fixture.port);
	support::send_requests(*stalled_client, support::make_get_request("/secure", "close"));

	const auto stalled_response = support::read_response(*stalled_client);
	EXPECT_EQ(stalled_response.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(stalled_response).at("route").as_string()), "secure");

	EXPECT_TRUE(support::read_until_eof(*stalled_client));
}

TYPED_TEST(HttpTlsIntegrationTest, PlainHttpClientToTlsServerIsRejectedAndServerRecovers) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	auto plain_client = support::connect_client(fixture.port);
	support::send_requests(*plain_client, support::make_get_request("/secure", "close"));
	EXPECT_TRUE(support::next_response_is_eof(*plain_client));

	auto tls_client = support::connect_tls_client(fixture.port);
	support::send_requests(*tls_client, support::make_get_request("/secure", "close"));
	const auto resp = support::read_response(*tls_client);
	EXPECT_EQ(resp.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(resp).at("route").as_string()), "secure");
	EXPECT_TRUE(support::read_until_eof(*tls_client));
}

TYPED_TEST(HttpTlsIntegrationTest, UntrustedTlsClientIsRejectedWithoutBreakingLaterTrustedClients) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	const auto handshake_error = support::connect_tls_client_without_trust(fixture.port);
	EXPECT_TRUE(static_cast<bool>(handshake_error));

	auto trusted_client = support::connect_tls_client(fixture.port);
	support::send_requests(*trusted_client, support::make_get_request("/secure", "close"));
	const auto resp = support::read_response(*trusted_client);
	EXPECT_EQ(resp.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(resp).at("route").as_string()), "secure");
	EXPECT_TRUE(support::read_until_eof(*trusted_client));
}

} // namespace warp::tests
