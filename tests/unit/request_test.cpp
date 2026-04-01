#include "warp/http/request.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http/verb.hpp>

namespace {

using warp::http::request;

TEST(RequestTest, ParsesPathAndDecodedQueryParameters) {
	request req(boost::beast::http::verb::get, "/items/42?lang=en&title=warp+server&escaped=%2Fv1%2Fping&empty=", 11);

	EXPECT_EQ(req.path(), "/items/42");
	ASSERT_TRUE(req.query_param("lang").has_value());
	EXPECT_EQ(*req.query_param("lang"), "en");
	ASSERT_TRUE(req.query_param("title").has_value());
	EXPECT_EQ(*req.query_param("title"), "warp server");
	ASSERT_TRUE(req.query_param("escaped").has_value());
	EXPECT_EQ(*req.query_param("escaped"), "/v1/ping");
	ASSERT_TRUE(req.query_param("empty").has_value());
	EXPECT_EQ(*req.query_param("empty"), "");
	EXPECT_FALSE(req.query_param("missing").has_value());
}

TEST(RequestTest, RefreshTargetMetadataReflectsTargetMutation) {
	request req(boost::beast::http::verb::get, "/before?x=1", 11);
	req.target("/after/99?x=9&mode=full");
	req.refresh_target_metadata();

	EXPECT_EQ(req.path(), "/after/99");
	ASSERT_TRUE(req.query_param("x").has_value());
	EXPECT_EQ(*req.query_param("x"), "9");
	ASSERT_TRUE(req.query_param("mode").has_value());
	EXPECT_EQ(*req.query_param("mode"), "full");
}

TEST(RequestTest, JsonBodyAndTryJsonBodyParseValidJson) {
	request req(boost::beast::http::verb::post, "/payload", 11);
	req.body() = R"({"answer":42,"name":"warp"})";

	auto parsed = req.json_body().as_object();
	EXPECT_EQ(parsed.at("answer").as_int64(), 42);
	EXPECT_EQ(parsed.at("name").as_string(), "warp");

	auto maybe = req.try_json_body();
	ASSERT_TRUE(maybe.has_value());
	EXPECT_EQ(maybe->as_object().at("answer").as_int64(), 42);
}

TEST(RequestTest, TryJsonBodyReturnsNulloptForInvalidJson) {
	request req(boost::beast::http::verb::post, "/payload", 11);
	req.body() = "not-json";

	EXPECT_THROW(static_cast<void>(req.json_body()), std::exception);
	EXPECT_FALSE(req.try_json_body().has_value());
}

TEST(RequestTest, PathParamLookupReadsAssignedPathParams) {
	request req(boost::beast::http::verb::get, "/users/7", 11);
	req.set_path_params({{"id", "7"}, {"kind", "admin"}});

	ASSERT_TRUE(req.path_param("id").has_value());
	EXPECT_EQ(*req.path_param("id"), "7");
	ASSERT_TRUE(req.path_param("kind").has_value());
	EXPECT_EQ(*req.path_param("kind"), "admin");
	EXPECT_FALSE(req.path_param("missing").has_value());
}

} // namespace
