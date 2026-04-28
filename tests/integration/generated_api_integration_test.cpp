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
	std::optional<std::string> filter;
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
		observed_->user_id = request.user_id;
		observed_->verbose = request.verbose;
		observed_->filter = request.filter;
		observed_->trace_id = request.x_trace_id;
		observed_->name = request.body.name;
		observed_->nickname = request.body.nickname;

		generated::users_create_user_response response;
		response.body.id = 42;
		response.body.active = true;
		return response;
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
		observed_->user_id = request.user_id;
		observed_->verbose = request.verbose;
		observed_->filter = request.filter;
		observed_->trace_id = request.x_trace_id;
		observed_->name = request.body.name;
		observed_->nickname = request.body.nickname;
		if (request.body.name == "Alice") {
			return warp::response::not_found("user '" + request.user_id + "' was not found");
		}
		generated::users_create_user_response response;
		response.body.id = 7;
		response.body.active = false;
		return response;
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
		fixture.emplace(warp::server::server_builder().register_resource(routes));
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string request_body = R"({"name":"Alice","nickname":"ally"})";
	const std::string payload = "POST /users/alice?verbose=true&filter=all HTTP/1.1\r\n"
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
	ASSERT_TRUE(observed->filter.has_value());
	EXPECT_EQ(*observed->filter, "all");
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
		fixture.emplace(warp::server::server_builder().register_resource(routes));
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
	EXPECT_TRUE(observed->user_id.empty());
	EXPECT_FALSE(observed->filter.has_value());
}

TEST(GeneratedApiIntegrationTest, ReturnsUnsupportedMediaTypeWhenJsonContentTypeIsMissing) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_resource>(observed);
	generated::users_api_routes<generated_users_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes));
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
	EXPECT_TRUE(observed->user_id.empty());
}

TEST(GeneratedApiIntegrationTest, AcceptsStructuredSyntaxJsonContentTypes) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_resource>(observed);
	generated::users_api_routes<generated_users_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes));
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
	                            "Content-Type: application/vnd.api+json; charset=utf-8\r\n"
	                            "x-trace-id: trace-123\r\n"
	                            "Content-Length: " +
	                            std::to_string(request_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            request_body;
	support::send_requests(*client, payload);

	const auto response = support::read_response(*client);
	const auto body = support::parse_object_body(response);

	EXPECT_EQ(response.result(), http::status::created);
	EXPECT_EQ(body.at("id").as_int64(), 42);
	EXPECT_TRUE(body.at("active").as_bool());
	EXPECT_EQ(observed->user_id, "alice");
	EXPECT_EQ(observed->trace_id, "trace-123");
	EXPECT_EQ(observed->name, "Alice");
	EXPECT_FALSE(observed->filter.has_value());
}

TEST(GeneratedApiIntegrationTest, ReturnsBadRequestForRequestValidationFailures) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_resource>(observed);
	generated::users_api_routes<generated_users_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes));
	} catch (const std::exception &ex) {
		if (std::string(ex.what()).find("Operation not permitted") != std::string::npos) {
			GTEST_SKIP() << ex.what();
		}
		throw;
	}

	auto client = support::connect_client(fixture->port);
	const std::string invalid_path_body = R"({"name":"Alice"})";
	const std::string invalid_query_body = R"({"name":"Alice"})";
	const std::string invalid_header_body = R"({"name":"Alice"})";
	const std::string invalid_json_body = R"({"name":"Al","nickname":"toolong"})";
	const std::string payload = "POST /users/ab HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: keep-alive\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: trace-123\r\n"
	                            "Content-Length: " +
	                            std::to_string(invalid_path_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            invalid_path_body +
	                            "POST /users/alice?filter=x HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: keep-alive\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: trace-123\r\n"
	                            "Content-Length: " +
	                            std::to_string(invalid_query_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            invalid_query_body +
	                            "POST /users/alice HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: keep-alive\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: x\r\n"
	                            "Content-Length: " +
	                            std::to_string(invalid_header_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            invalid_header_body +
	                            "POST /users/alice HTTP/1.1\r\n"
	                            "Host: 127.0.0.1\r\n"
	                            "Connection: close\r\n"
	                            "Content-Type: application/json\r\n"
	                            "x-trace-id: trace-123\r\n"
	                            "Content-Length: " +
	                            std::to_string(invalid_json_body.size()) +
	                            "\r\n"
	                            "\r\n" +
	                            invalid_json_body;
	support::send_requests(*client, payload);

	const auto invalid_path = support::read_response(*client);
	const auto invalid_query = support::read_response(*client);
	const auto invalid_header = support::read_response(*client);
	const auto invalid_json = support::read_response(*client);
	const auto invalid_path_body_json = support::parse_object_body(invalid_path);
	const auto invalid_query_body_json = support::parse_object_body(invalid_query);
	const auto invalid_header_body_json = support::parse_object_body(invalid_header);
	const auto invalid_json_body_json = support::parse_object_body(invalid_json);

	EXPECT_EQ(invalid_path.result(), http::status::bad_request);
	EXPECT_EQ(std::string(invalid_path_body_json.at("error").as_string()),
	          "invalid path parameter 'user_id': length must be >= 3");

	EXPECT_EQ(invalid_query.result(), http::status::bad_request);
	EXPECT_EQ(std::string(invalid_query_body_json.at("error").as_string()),
	          "invalid query parameter 'filter': length must be >= 2");

	EXPECT_EQ(invalid_header.result(), http::status::bad_request);
	EXPECT_EQ(std::string(invalid_header_body_json.at("error").as_string()),
	          "invalid header 'x-trace-id': length must be >= 3");

	EXPECT_EQ(invalid_json.result(), http::status::bad_request);
	EXPECT_EQ(std::string(invalid_json_body_json.at("error").as_string()),
	          "invalid JSON body field 'name': length must be >= 3");

	EXPECT_TRUE(observed->user_id.empty());
	EXPECT_FALSE(observed->filter.has_value());
	EXPECT_TRUE(support::read_until_eof(*client));
}

TEST(GeneratedApiIntegrationTest, AllowsGeneratedServicesToReturnMixedTypedAndHttpResponses) {
	auto observed = std::make_shared<observed_request_state>();
	auto service = std::make_shared<generated_users_error_resource>(observed);
	generated::users_api_routes<generated_users_error_resource> routes(service);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::server::server_builder().register_resource(routes));
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
	EXPECT_FALSE(observed->filter.has_value());
	EXPECT_EQ(observed->trace_id, "trace-404");
	EXPECT_EQ(observed->name, "Alice");
	EXPECT_FALSE(observed->nickname.has_value());
}

} // namespace warp::tests
