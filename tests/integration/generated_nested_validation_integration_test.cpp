#include "generated_nested_validation_api_resources.hpp"
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
namespace generated = ::generated_nested_api;

class generated_nested_validation_resource {
public:
	generated::batches_create_batch_response create_batch(generated::batches_create_batch_request) {
		generated::batches_create_batch_response response;
		response.body.accepted = true;
		return response;
	}
};

TEST(GeneratedNestedValidationIntegrationTest, ReportsNestedArrayObjectValidationPaths) {
	auto service = std::make_shared<generated_nested_validation_resource>();
	generated::batches_api_routes<generated_nested_validation_resource> routes(service);

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
	const std::string request_body = R"({"items":[{"name":"x"}]})";
	const std::string payload = "POST /batches HTTP/1.1\r\n"
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
	EXPECT_EQ(std::string(body.at("error").as_string()),
	          "invalid JSON body field 'items[0].name': length must be >= 3");
}

} // namespace warp::tests
