#include "generated_users_api_resources.hpp"
#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <string>

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

class generated_users_resource : public generated::users_api_base<generated_users_resource> {
public:
	explicit generated_users_resource(std::shared_ptr<observed_request_state> observed)
	    : observed_(std::move(observed)) {
	}

	generated::users_create_user_response create_user(generated::users_create_user_request request) {
		observed_->user_id = request.user_id;
		observed_->verbose = request.verbose;
		observed_->trace_id = request.x_trace_id;
		observed_->name = request.body.name;
		observed_->nickname = request.body.nickname;

		generated::users_create_user_response response;
		response.body.id = 42;
		response.body.active = true;
		return response;
	}

	warp::awaitable<generated::users_health_response> health(generated::users_health_request) {
		co_return generated::users_health_response {};
	}

private:
	std::shared_ptr<observed_request_state> observed_;
};

TEST(GeneratedApiIntegrationTest, ParsesTypedRequestsAndSerializesTypedResponses) {
	auto observed = std::make_shared<observed_request_state>();
	generated_users_resource resource(observed);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::http::server_builder().register_resource(resource));
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
	generated_users_resource resource(observed);

	std::optional<support::server_fixture> fixture;
	try {
		fixture.emplace(warp::http::server_builder().register_resource(resource));
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

} // namespace warp::tests
