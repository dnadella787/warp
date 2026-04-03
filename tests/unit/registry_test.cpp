#include "http/router/registry.hpp"
#include "warp/codegen/http_adapter.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>

#include "support/test_helpers.hpp"

namespace {

using boost::beast::http::status;
using boost::beast::http::verb;
using warp::http::registry;

struct path_contract {
	std::string id;
};

} // namespace

namespace warp::codegen {

template <>
struct request_contract_traits<::path_contract> {
	static parse_result<::path_contract> parse(const warp::http::request &req) {
		auto id = required_path_param<std::string>(req, "id");
		if (!id.has_value()) {
			return parse_result<::path_contract>::failure(id.error());
		}
		return parse_result<::path_contract>::success({.id = std::move(id).value()});
	}
};

} // namespace warp::codegen

namespace {

TEST(RegistryTest, FindMatchesLiteralRouteAndMethod) {
	registry routes;
	routes.add(warp::method::get, "/health", [](const warp::request &) -> warp::response {
		return warp::response::ok(warp::body_builder().set("route", "health").build());
	});

	warp::request req(verb::get, "/health", 11);
	auto handler = routes.find(req);
	ASSERT_NE(handler, nullptr);

	auto resp = warp::test::run_handler(*handler, std::move(req));
	auto body = warp::test::parse_json_object(resp.body());
	EXPECT_EQ(resp.result(), status::ok);
	EXPECT_EQ(body.at("route").as_string(), "health");
}

TEST(RegistryTest, FindAppliesPathParametersForParameterizedRoutes) {
	registry routes;
	routes.add(warp::method::get, "/users/{id}", [](const warp::request &req) -> warp::response {
		return warp::response::ok(
		    warp::body_builder().set("id", std::string(req.path_param("id").value_or(""))).build());
	});

	warp::request req(verb::get, "/users/42?verbose=true", 11);
	auto handler = routes.find(req);
	ASSERT_NE(handler, nullptr);
	ASSERT_TRUE(req.path_param("id").has_value());
	EXPECT_EQ(*req.path_param("id"), "42");

	auto resp = warp::test::run_handler(*handler, std::move(req));
	auto body = warp::test::parse_json_object(resp.body());
	EXPECT_EQ(body.at("id").as_string(), "42");
}

TEST(RegistryTest, FindPercentDecodesPathParameters) {
	registry routes;
	routes.add(warp::method::get, "/files/{path}", [](const warp::request &req) -> warp::response {
		return warp::response::ok(
		    warp::body_builder().set("path", std::string(req.path_param("path").value_or(""))).build());
	});

	warp::request req(verb::get, "/files/%2Fv1%2Fping", 11);
	auto handler = routes.find(req);
	ASSERT_NE(handler, nullptr);
	ASSERT_TRUE(req.path_param("path").has_value());
	EXPECT_EQ(*req.path_param("path"), "/v1/ping");
}

TEST(RegistryTest, FindRejectsMalformedPercentEncodingInPathParameters) {
	registry routes;
	routes.add(warp::method::get, "/files/{id}",
	           [](const warp::request &) -> warp::response { return warp::response::ok(); });

	warp::request req(verb::get, "/files/%ZZ", 11);
	auto handler = routes.find(req);
	ASSERT_NE(handler, nullptr);

	const auto parsed = warp::codegen::parse_http_request<path_contract>(req);
	ASSERT_FALSE(parsed.has_value());
	EXPECT_EQ(parsed.error().status, status::bad_request);
	EXPECT_EQ(parsed.error().code, "malformed_path_parameter");
	EXPECT_EQ(parsed.error().message, "malformed percent-encoding in path parameter 'id'");
}

TEST(RegistryTest, FindKeepsPlusSignsLiteralInPathParameters) {
	registry routes;
	routes.add(warp::method::get, "/files/{path}", [](const warp::request &req) -> warp::response {
		return warp::response::ok(
		    warp::body_builder().set("path", std::string(req.path_param("path").value_or(""))).build());
	});

	warp::request req(verb::get, "/files/a+b", 11);
	auto handler = routes.find(req);
	ASSERT_NE(handler, nullptr);
	ASSERT_TRUE(req.path_param("path").has_value());
	EXPECT_EQ(*req.path_param("path"), "a+b");
}

TEST(RegistryTest, FindPrefersLiteralRouteOverParameterRoute) {
	registry routes;
	routes.add(warp::method::get, "/users/{id}", [](const warp::request &) -> warp::response {
		return warp::response::ok(warp::body_builder().set("route", "parameter").build());
	});
	routes.add(warp::method::get, "/users/me", [](const warp::request &) -> warp::response {
		return warp::response::ok(warp::body_builder().set("route", "literal").build());
	});

	warp::request req(verb::get, "/users/me", 11);
	auto handler = routes.find(req);
	ASSERT_NE(handler, nullptr);

	auto resp = warp::test::run_handler(*handler, std::move(req));
	auto body = warp::test::parse_json_object(resp.body());
	EXPECT_EQ(body.at("route").as_string(), "literal");
}

TEST(RegistryTest, FindReturnsNullForUnknownMethodOrPath) {
	registry routes;
	routes.add(warp::method::get, "/users/{id}",
	           [](const warp::request &) -> warp::response { return warp::response::ok(); });

	warp::request wrong_method(verb::post, "/users/42", 11);
	warp::request wrong_path(verb::get, "/accounts/42", 11);

	EXPECT_EQ(routes.find(wrong_method), nullptr);
	EXPECT_EQ(routes.find(wrong_path), nullptr);
}

TEST(RegistryTest, FindClearsStalePathParamsOnMiss) {
	registry routes;
	routes.add(warp::method::get, "/users/{id}",
	           [](const warp::request &) -> warp::response { return warp::response::ok(); });

	warp::request req(verb::get, "/users/42", 11);
	ASSERT_NE(routes.find(req), nullptr);
	ASSERT_TRUE(req.path_param("id").has_value());
	EXPECT_EQ(*req.path_param("id"), "42");

	req.target("/accounts/42");
	req.refresh_target_metadata();
	EXPECT_EQ(routes.find(req), nullptr);
	EXPECT_FALSE(req.path_param("id").has_value());
}

TEST(RegistryTest, AddRejectsInvalidRoutePatterns) {
	registry routes;
	auto handler = [](const warp::request &) -> warp::response {
		return warp::response::ok();
	};

	EXPECT_THROW(routes.add(warp::method::get, "users/{id}", handler), std::invalid_argument);
	EXPECT_THROW(routes.add(warp::method::get, "/users//id", handler), std::invalid_argument);
	EXPECT_THROW(routes.add(warp::method::get, "/users/{}", handler), std::invalid_argument);
}

TEST(RegistryTest, AddRejectsDuplicateNormalizedRouteShapesPerMethod) {
	registry routes;
	auto handler = [](const warp::request &) -> warp::response {
		return warp::response::ok();
	};

	routes.add(warp::method::get, "/users/{id}", handler);
	EXPECT_THROW(routes.add(warp::method::get, "/users/{name}", handler), std::invalid_argument);
	routes.add(warp::method::post, "/users/{name}", handler);
}

TEST(RegistryTest, CopyConstructorPreservesRouteTree) {
	registry original;
	original.add(warp::method::get, "/copy/{id}", [](const warp::request &req) -> warp::response {
		return warp::response::ok(
		    warp::body_builder().set("id", std::string(req.path_param("id").value_or(""))).build());
	});

	registry copied = original;
	warp::request req(verb::get, "/copy/77", 11);
	auto handler = copied.find(req);
	ASSERT_NE(handler, nullptr);

	auto resp = warp::test::run_handler(*handler, std::move(req));
	auto body = warp::test::parse_json_object(resp.body());
	EXPECT_EQ(body.at("id").as_string(), "77");
}

} // namespace
