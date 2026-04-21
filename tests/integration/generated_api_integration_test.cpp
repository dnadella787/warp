#include "generated_users_api_resources.hpp"
#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <string>

#include "warp/warp.hpp"

namespace warp::tests {

namespace http = boost::beast::http;
namespace support = integration_support;
namespace generated = ::generated_api;

struct observed_request_state {
	std::string user_id;
	std::optional<bool> verbose;
	std::string trace_id;
	std::string name;
	std::optional<std::string> nickname;
};

class generated_users_resource {
public:
	explicit generated_users_resource(std::shared_ptr<observed_request_state> observed)
	    : observed_(std::move(observed)) {
	}

	generated::users_create_user_response create_user(generated::users_create_user_request request) {
		observed_->user_id = request.user_id();
		observed_->verbose = request.verbose();
		observed_->trace_id = request.x_trace_id();
		observed_->name = request.body().name();
		observed_->nickname = request.body().nickname();

		return generated::users_create_user_response::builder()
		    .body(generated::users_create_user_response_body::builder().id(42).active(true).build())
		    .build();
	}

	awaitable<generated::users_health_response> health(generated::users_health_request) {
		co_return generated::users_health_response {};
	}

private:
	std::shared_ptr<observed_request_state> observed_;
};

class generated_users_error_resource {
public:
	explicit generated_users_error_resource(std::shared_ptr<observed_request_state> observed)
	    : observed_(std::move(observed)) {
	}

	generated::users_create_user_request_handler_result create_user(generated::users_create_user_request request) {
		observed_->user_id = request.user_id();
		observed_->verbose = request.verbose();
		observed_->trace_id = request.x_trace_id();
		observed_->name = request.body().name();
		observed_->nickname = request.body().nickname();
		if (request.body().name() == "Alice") {
			return warp::response::not_found("user '" + request.user_id() + "' was not found");
		}
		return generated::users_create_user_response::builder()
		    .body(generated::users_create_user_response_body::builder().id(7).active(false).build())
		    .build();
	}

	awaitable<generated::users_health_request_handler_result> health(generated::users_health_request) {
		co_return warp::response::server_error("health backend failed");
	}

private:
	std::shared_ptr<observed_request_state> observed_;
};

TEST(GeneratedApiIntegrationTest, ParsesTypedRequestsAndSerializesTypedResponses) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_resource>(observed);
	generated::users_api_routes<generated_users_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::http::server_builder().register_resource(routes));
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string request_body = R"({"name":"Alice","nickname":"ally"})";
	const std::string payload = "POST /users/alice?verbose=true HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: keep-alive\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: trace-123\r\n"
	                            "Content-Length: " +
	                            std::to_string(request_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            request_body +
	                            "GET /health HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: close\r\n"
	                            "\r\n";
	support::send_requests(*client, payload);

	const auto created = support::read_response(*client);
	const auto health = support::read_response(*client);
	const auto created_body = support::parse_object_body(created);

	EXPECT_EQ(created.result(), http::status::created);
	EXPECT_EQ(created_body.at("id").as_int64(), 42);
	EXPECT_TRUE(created_body.at("active").as_bool());
	EXPECT_EQ(observed->user_id, "alice");
	ASSERT_TRUE(observed->verbose.has_value());
	EXPECT_TRUE(*observed->verbose);
	EXPECT_EQ(observed->trace_id, "trace-123");
	EXPECT_EQ(observed->name, "Alice");
	ASSERT_TRUE(observed->nickname.has_value());
	EXPECT_EQ(*observed->nickname, "ally");

	EXPECT_EQ(health.result(), http::status::no_content);
	EXPECT_TRUE(health.body().empty());
}

TEST(GeneratedApiIntegrationTest, ReturnsBadRequestForBindingFailures) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_resource>(observed);
	generated::users_api_routes<generated_users_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::http::server_builder().register_resource(routes));
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string request_body = R"({"name":"Alice"})";
	const std::string payload = "POST /users/alice HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: close\r\n"
	                            "Content-Type: application/json\r\n"
	                            "Content-Length: " +
	                            std::to_string(request_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            request_body;
	support::send_requests(*client, payload);

	const auto response = support::read_response(*client);
	const auto body = support::parse_object_body(response);

	EXPECT_EQ(response.result(), http::status::bad_request);
	EXPECT_EQ(std::string(body.at("error").as_string()), "missing required header 'x-trace-id'");
}

TEST(GeneratedApiIntegrationTest, ReturnsUnsupportedMediaTypeWhenJsonContentTypeIsMissing) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_resource>(observed);
	generated::users_api_routes<generated_users_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::http::server_builder().register_resource(routes));
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string request_body = R"({"name":"Alice"})";
	const std::string payload = "POST /users/alice HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: close\r\n"
	                            "x-trace-id: trace-123\r\n"
	                            "Content-Length: " +
	                            std::to_string(request_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            request_body;
	support::send_requests(*client, payload);

	const auto response = support::read_response(*client);
	const auto body = support::parse_object_body(response);

	EXPECT_EQ(response.result(), http::status::unsupported_media_type);
	EXPECT_EQ(std::string(body.at("error").as_string()), "expected Content-Type application/json");
}

TEST(GeneratedApiIntegrationTest, AllowsGeneratedServicesToReturnMixedTypedAndHttpResponses) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_error_resource>(observed);
	generated::users_api_routes<generated_users_error_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::http::server_builder().register_resource(routes));
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string success_body = R"({"name":"Bob"})";
	const std::string missing_body_json = R"({"name":"Alice"})";
	const std::string payload = "POST /users/bob?verbose=true HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: keep-alive\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: trace-200\r\n"
	                            "Content-Length: " +
	                            std::to_string(success_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            success_body +
	                            "POST /users/alice?verbose=false HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: keep-alive\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: trace-404\r\n"
	                            "Content-Length: " +
	                            std::to_string(missing_body_json.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            missing_body_json +
	                            "GET /health HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: close\r\n"
	                            "\r\n";
	support::send_requests(*client, payload);

	const auto created = support::read_response(*client);
	const auto missing = support::read_response(*client);
	const auto failing_health = support::read_response(*client);
	const auto created_body = support::parse_object_body(created);
	const auto missing_body = support::parse_object_body(missing);
	const auto health_body = support::parse_object_body(failing_health);

	EXPECT_EQ(created.result(), http::status::created);
	EXPECT_EQ(created_body.at("id").as_int64(), 7);
	EXPECT_FALSE(created_body.at("active").as_bool());
	EXPECT_EQ(missing.result(), http::status::not_found);
	EXPECT_EQ(std::string(missing_body.at("error").as_string()), "user 'alice' was not found");
	EXPECT_EQ(failing_health.result(), http::status::internal_server_error);
	EXPECT_EQ(std::string(health_body.at("error").as_string()), "health backend failed");
	EXPECT_EQ(observed->user_id, "alice");
	ASSERT_TRUE(observed->verbose.has_value());
	EXPECT_FALSE(*observed->verbose);
	EXPECT_EQ(observed->trace_id, "trace-404");
	EXPECT_EQ(observed->name, "Alice");
	EXPECT_FALSE(observed->nickname.has_value());
}

} // namespace warp::tests
