#include "warp/http/body_builder.hpp"
#include "warp/http/response_builder.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>

#include "support/test_helpers.hpp"

namespace {

using boost::beast::http::field;
using boost::beast::http::status;

TEST(BodyBuilderSmokeTest, BuildsExpectedJsonShape) {
	auto body = warp::body_builder().set("name", "warp").set("count", 2).set("ready", true).build();
	auto parsed = warp::test::parse_json_object(body);

	EXPECT_EQ(parsed.at("name").as_string(), "warp");
	EXPECT_EQ(parsed.at("count").as_int64(), 2);
	EXPECT_TRUE(parsed.at("ready").as_bool());
}

TEST(ResponseBuilderSmokeTest, ComposesStatusHeadersAndJsonBody) {
	auto body = warp::body_builder().set("id", 10).set("state", "created");
	auto response = warp::response_builder()
	                    .status(status::created)
	                    .body(body)
	                    .header("x-request-id", "abc-123")
	                    .keep_alive(false)
	                    .build();

	auto parsed = warp::test::parse_json_object(response.body());
	EXPECT_EQ(response.result(), status::created);
	EXPECT_EQ(response[field::content_type], "application/json");
	EXPECT_EQ(response[field::connection], "close");
	const auto request_id = response.find("x-request-id");
	ASSERT_NE(request_id, response.end());
	EXPECT_EQ(request_id->value(), "abc-123");
	EXPECT_EQ(parsed.at("id").as_int64(), 10);
	EXPECT_EQ(parsed.at("state").as_string(), "created");
}

} // namespace
