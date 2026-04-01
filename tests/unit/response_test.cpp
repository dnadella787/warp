#include "warp/http/response.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/json/object.hpp>

#include "support/test_helpers.hpp"

namespace {

using boost::beast::http::field;
using boost::beast::http::status;
using warp::http::response;

TEST(ResponseTest, OkUsesJsonContentTypeByDefault) {
	auto resp = response::ok(R"({"ok":true})");

	EXPECT_EQ(resp.result(), status::ok);
	EXPECT_EQ(resp[field::content_type], "application/json");
	EXPECT_EQ(resp.body(), R"({"ok":true})");
}

TEST(ResponseTest, NoContentHasNoBodyAndNoContentTypeHeader) {
	auto resp = response::no_content();

	EXPECT_EQ(resp.result(), status::no_content);
	EXPECT_TRUE(resp.body().empty());
	EXPECT_EQ(resp.find(field::content_type), resp.end());
}

TEST(ResponseTest, ErrorHelpersEncodeMessageAsJsonPayload) {
	auto resp = response::bad_request("missing id");
	auto body = warp::test::parse_json_object(resp.body());

	EXPECT_EQ(resp.result(), status::bad_request);
	EXPECT_EQ(resp[field::content_type], "application/json");
	EXPECT_EQ(body.at("error").as_string(), "missing id");
}

TEST(ResponseTest, JsonValueOverloadSerializesObjectBody) {
	boost::json::object payload;
	payload["name"] = "warp";
	payload["ready"] = true;

	auto resp = response::ok(boost::json::value(payload));
	auto body = warp::test::parse_json_object(resp.body());

	EXPECT_EQ(resp.result(), status::ok);
	EXPECT_EQ(resp[field::content_type], "application/json");
	EXPECT_EQ(body.at("name").as_string(), "warp");
	EXPECT_TRUE(body.at("ready").as_bool());
}

TEST(ResponseTest, CreatedAndAcceptedSetExpectedStatusCodes) {
	auto created = response::created(R"({"id":1})");
	auto accepted = response::accepted(R"({"queued":true})");

	EXPECT_EQ(created.result(), status::created);
	EXPECT_EQ(accepted.result(), status::accepted);
}

} // namespace
