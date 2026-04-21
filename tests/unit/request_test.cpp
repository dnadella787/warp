#include "warp/codegen/http_adapter.hpp"
#include "warp/http/request.hpp"

#include <gtest/gtest.h>

#include <boost/json/src.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/object.hpp>

namespace {

using boost::beast::http::status;
using warp::http::request;

struct query_contract {
	bool verbose {};
};

} // namespace

namespace warp::codegen {

template <>
struct request_contract_traits<::query_contract> {
	static parse_result<::query_contract> parse(const warp::http::request &req) {
		auto verbose = required_query_param<bool>(req, "verbose");
		if (!verbose.has_value()) {
			return parse_result<::query_contract>::failure(verbose.error());
		}
		return parse_result<::query_contract>::success({.verbose = std::move(verbose).value()});
	}
};

} // namespace warp::codegen

namespace {

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
	req.set_path_params({{"id", "before"}});
	req.target("/after/99?x=9&mode=full");
	req.refresh_target_metadata();

	EXPECT_EQ(req.path(), "/after/99");
	ASSERT_TRUE(req.query_param("x").has_value());
	EXPECT_EQ(*req.query_param("x"), "9");
	ASSERT_TRUE(req.query_param("mode").has_value());
	EXPECT_EQ(*req.query_param("mode"), "full");
	EXPECT_FALSE(req.path_param("id").has_value());
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

	EXPECT_ANY_THROW(static_cast<void>(req.json_body()));
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

TEST(RequestTest, QueryAndPathLookupSupportConstCharStringAndStringViewKeys) {
	request req(boost::beast::http::verb::get, "/items/42?lang=en", 11);
	req.set_path_params({{"id", "42"}});

	const std::string query_key = "lang";
	const std::string query_key_storage = "lang-extra";
	const std::string path_key = "id";
	const std::string path_key_storage = "id-extra";

	const std::string_view query_key_view {query_key_storage.data(), query_key.size()};
	const std::string_view path_key_view {path_key_storage.data(), path_key.size()};

	EXPECT_EQ(req.query_param("lang"), "en");
	EXPECT_EQ(req.query_param(query_key), "en");
	EXPECT_EQ(req.query_param(query_key_view), "en");

	const auto query_it = req.query_params().find(query_key_view);
	ASSERT_NE(query_it, req.query_params().end());
	EXPECT_EQ(query_it->second, "en");

	EXPECT_EQ(req.path_param("id"), "42");
	EXPECT_EQ(req.path_param(path_key), "42");
	EXPECT_EQ(req.path_param(path_key_view), "42");

	const auto path_it = req.path_params().find(path_key_view);
	ASSERT_NE(path_it, req.path_params().end());
	EXPECT_EQ(path_it->second, "42");
}

TEST(RequestTest, JsonBodyRejectsMissingOrInvalidJsonContentTypes) {
	request missing_header(boost::beast::http::verb::post, "/payload", 11);
	missing_header.body() = R"({"name":"warp"})";

	const auto missing_result = warp::codegen::json_body<boost::json::object>(missing_header);
	ASSERT_FALSE(missing_result.has_value());
	EXPECT_EQ(missing_result.error().status, status::unsupported_media_type);
	EXPECT_EQ(missing_result.error().code, "unsupported_media_type");

	request invalid_type(boost::beast::http::verb::post, "/payload", 11);
	invalid_type.set("Content-Type", "application/jsonx");
	invalid_type.body() = R"({"name":"warp"})";

	const auto invalid_result = warp::codegen::json_body<boost::json::object>(invalid_type);
	ASSERT_FALSE(invalid_result.has_value());
	EXPECT_EQ(invalid_result.error().status, status::unsupported_media_type);
	EXPECT_EQ(invalid_result.error().code, "unsupported_media_type");
}

TEST(RequestTest, JsonBodyAcceptsJsonSubtypesAndRejectsMalformedJsonSeparately) {
	request subtype(boost::beast::http::verb::post, "/payload", 11);
	subtype.set("Content-Type", "Application/Vnd.Api+Json ; charset=utf-8");
	subtype.body() = R"({"name":"warp"})";

	const auto subtype_result = warp::codegen::json_body<boost::json::object>(subtype);
	ASSERT_TRUE(subtype_result.has_value());
	EXPECT_EQ(subtype_result.value().at("name").as_string(), "warp");

	request malformed(boost::beast::http::verb::post, "/payload", 11);
	malformed.set("Content-Type", "application/json");
	malformed.body() = "{";

	const auto malformed_result = warp::codegen::json_body<boost::json::object>(malformed);
	ASSERT_FALSE(malformed_result.has_value());
	EXPECT_EQ(malformed_result.error().status, status::bad_request);
	EXPECT_EQ(malformed_result.error().code, "invalid_json");
}

TEST(RequestTest, ParseHttpRequestRejectsDuplicateQueryParameters) {
	request req(boost::beast::http::verb::get, "/items?verbose=false&verbose=true", 11);

	const auto parsed = warp::codegen::parse_http_request<query_contract>(req);
	ASSERT_FALSE(parsed.has_value());
	EXPECT_EQ(parsed.error().status, status::bad_request);
	EXPECT_EQ(parsed.error().code, "duplicate_query_parameter");
	EXPECT_EQ(parsed.error().message, "duplicate query parameter 'verbose'");
}

TEST(RequestTest, ParseHttpRequestRejectsMalformedQueryPercentEncoding) {
	request req(boost::beast::http::verb::get, "/items?verbose=%ZZ", 11);

	const auto parsed = warp::codegen::parse_http_request<query_contract>(req);
	ASSERT_FALSE(parsed.has_value());
	EXPECT_EQ(parsed.error().status, status::bad_request);
	EXPECT_EQ(parsed.error().code, "malformed_query_parameter");
	EXPECT_EQ(parsed.error().message, "malformed percent-encoding in query parameter 'verbose'");
}

} // namespace
