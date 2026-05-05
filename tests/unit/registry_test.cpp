#include "server/router/registry.hpp"
#include "server/router/route_pattern.hpp"
#include "warp/codegen/http_adapter.hpp"
#include "warp/server/router/route_spec.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace {

using boost::beast::http::status;
using boost::beast::http::verb;
using warp::server::registry;

struct path_contract {
	std::string id;
};

using typed_summary_fields_route =
    warp::http::route_spec<warp::method::get, "/reports/{id}", warp::http::required_query<"summary">,
                           warp::http::required_query<"fields">>;
using typed_fields_summary_route =
    warp::http::route_spec<warp::method::get, "/reports/{report_id}", warp::http::required_query<"fields">,
                           warp::http::required_query<"summary">>;
using typed_summary_route =
    warp::http::route_spec<warp::method::get, "/reports/{id}", warp::http::required_query<"summary">,
                           warp::http::forbidden_query<"fields">>;
using typed_projection_route =
    warp::http::route_spec<warp::method::get, "/reports/{id}", warp::http::forbidden_query<"summary">,
                           warp::http::required_query<"fields">>;
using typed_summary_projection_route =
    warp::http::route_spec<warp::method::get, "/reports/{id}", warp::http::required_query<"summary">,
                           warp::http::required_query<"fields">>;
using typed_fallback_route = warp::http::route_spec<warp::method::get, "/reports/{id}">;
using typed_exact_mode_route =
    warp::http::route_spec<warp::method::get, "/items", warp::http::required_query_value<"mode", "full">>;
using typed_broad_mode_route = warp::http::route_spec<warp::method::get, "/items", warp::http::required_query<"mode">>;
using typed_encoded_query_route =
    warp::http::route_spec<warp::method::get, "/filters", warp::http::optional_query_value<"plus+space %", "a+b %">>;

struct parsed_query_constraint {
	std::string name;
	warp::http::query_constraint_presence presence {warp::http::query_constraint_presence::required};
	std::optional<std::string> exact_value;
};

std::vector<parsed_query_constraint> parse_query_constraints(std::string_view route) {
	std::vector<parsed_query_constraint> constraints;
	const auto query_pos = route.find('?');
	if (query_pos == std::string_view::npos) {
		return constraints;
	}

	const auto raw_query = route.substr(query_pos + 1);
	std::size_t start = 0;
	while (start < raw_query.size()) {
		const auto end = raw_query.find('&', start);
		const auto token =
		    raw_query.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!token.empty()) {
			const auto eq = token.find('=');
			auto key = warp::http::try_decode_query_component(token.substr(0, eq));
			if (!key.has_value() || key->empty()) {
				throw std::invalid_argument("route query constraint names must be non-empty and valid");
			}

			auto presence = warp::http::query_constraint_presence::required;
			if (key->front() == '!') {
				presence = warp::http::query_constraint_presence::forbidden;
				key->erase(key->begin());
			} else if (key->front() == '~') {
				presence = warp::http::query_constraint_presence::optional;
				key->erase(key->begin());
			}
			if (key->empty()) {
				throw std::invalid_argument("route query constraint names must be non-empty and valid");
			}

			const auto raw_value = eq == std::string_view::npos
			                           ? std::optional<std::string> {}
			                           : warp::http::try_decode_query_component(token.substr(eq + 1));
			if (eq != std::string_view::npos && !raw_value.has_value()) {
				throw std::invalid_argument("route query constraint values must use valid percent-encoding");
			}

			constraints.push_back(parsed_query_constraint {
			    .name = std::move(*key),
			    .presence = presence,
			    .exact_value = std::move(raw_value),
			});
		}

		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	return constraints;
}

registry::route_id add_route(registry &routes, warp::method verb, std::string_view path) {
	auto owned_constraints = parse_query_constraints(path);
	std::vector<warp::http::query_constraint_descriptor> descriptors;
	descriptors.reserve(owned_constraints.size());
	for (const auto &constraint : owned_constraints) {
		descriptors.push_back(warp::http::query_constraint_descriptor {
		    .name = constraint.name,
		    .presence = constraint.presence,
		    .exact_value = constraint.exact_value,
		});
	}
	return routes.add(verb, std::string(warp::http::strip_query_string(path)), descriptors);
}

template <typename Spec>
registry::route_id add_typed_route(registry &routes) {
	return routes.add(Spec::verb, std::string(Spec::path_view()),
	                  std::vector<warp::http::query_constraint_descriptor>(Spec::query_constraints.begin(),
	                                                                       Spec::query_constraints.end()));
}

void record_route_name(std::vector<std::string> &route_names, registry::route_id id, std::string name) {
	if (route_names.size() <= id.index()) {
		route_names.resize(id.index() + 1);
	}
	route_names[id.index()] = std::move(name);
}

std::string matched_route_name(registry &routes, const std::vector<std::string> &route_names, warp::request req) {
	const auto match = routes.find(req);
	if (!match.has_value()) {
		return {};
	}
	return route_names.at(match->id.index());
}

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
	const auto health_id = add_route(routes, warp::method::get, "/health");

	warp::request req(verb::get, "/health", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, health_id);
}

TEST(RegistryTest, FindAppliesPathParametersForParameterizedRoutes) {
	registry routes;
	const auto user_route_id = add_route(routes, warp::method::get, "/users/{id}");

	warp::request req(verb::get, "/users/42?verbose=true", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, user_route_id);
	ASSERT_TRUE(req.path_param(std::string_view {"id"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"id"}), "42");
	ASSERT_NE(req.path_params().find(std::string_view {"id"}), req.path_params().end());
	EXPECT_EQ(req.path_params().find(std::string_view {"id"})->second, "42");
}

TEST(RegistryTest, FindPercentDecodesPathParameters) {
	registry routes;
	const auto file_route_id = add_route(routes, warp::method::get, "/files/{path}");

	warp::request req(verb::get, "/files/%2Fv1%2Fping", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, file_route_id);
	ASSERT_TRUE(req.path_param(std::string_view {"path"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"path"}), "/v1/ping");
}

TEST(RegistryTest, FindRejectsMalformedPercentEncodingInPathParameters) {
	registry routes;
	const auto file_route_id = add_route(routes, warp::method::get, "/files/{id}");

	warp::request req(verb::get, "/files/%ZZ", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, file_route_id);

	const auto parsed = warp::codegen::parse_http_request<path_contract>(req);
	ASSERT_FALSE(parsed.has_value());
	EXPECT_EQ(parsed.error().status, status::bad_request);
	EXPECT_EQ(parsed.error().code, "malformed_path_parameter");
	EXPECT_EQ(parsed.error().message, "malformed percent-encoding in path parameter 'id'");
}

TEST(RegistryTest, FindKeepsPlusSignsLiteralInPathParameters) {
	registry routes;
	const auto file_route_id = add_route(routes, warp::method::get, "/files/{path}");

	warp::request req(verb::get, "/files/a+b", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, file_route_id);
	ASSERT_TRUE(req.path_param(std::string_view {"path"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"path"}), "a+b");
}

TEST(RegistryTest, FindPrefersLiteralRouteOverParameterRoute) {
	registry routes;
	const auto parameter_id = add_route(routes, warp::method::get, "/users/{id}");
	const auto literal_id = add_route(routes, warp::method::get, "/users/me");

	warp::request req(verb::get, "/users/me", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, literal_id);
	EXPECT_NE(match->id, parameter_id);
}

TEST(RegistryTest, FindFallsBackToParameterSiblingWhenLiteralQueryRouteDoesNotMatch) {
	registry routes;
	const auto literal_summary_id = add_route(routes, warp::method::get, "/users/me?summary");
	const auto parameter_id = add_route(routes, warp::method::get, "/users/{id}");

	warp::request req(verb::get, "/users/me", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, parameter_id);
	EXPECT_NE(match->id, literal_summary_id);
	ASSERT_TRUE(req.path_param(std::string_view {"id"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"id"}), "me");
}

TEST(RegistryTest, FindPrefersMostSpecificQueryAwareRoute) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}"), "fallback");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary"), "summary");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?fields"), "projection");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary&fields"),
	                  "summary_projection");

	warp::request req(verb::get, "/reports/42?summary=true&fields=name", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	ASSERT_TRUE(req.path_param(std::string_view {"id"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"id"}), "42");
	EXPECT_EQ(route_names.at(match->id.index()), "summary_projection");
}

TEST(RegistryTest, FindSelectsMatchingSingleQueryRouteBeforeFallback) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}"), "fallback");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary"), "summary");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?fields"), "projection");

	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?summary=true", 11)),
	          "summary");
	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?fields=name", 11)),
	          "projection");
}

TEST(RegistryTest, FindFallsBackWhenNoQueryAwareRouteMatches) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}"), "fallback");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary"), "summary");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?fields"), "projection");

	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?unused=1", 11)),
	          "fallback");
}

TEST(RegistryTest, AddCompiledPreservesTypedQuerySpecificity) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_typed_route<typed_fallback_route>(routes), "fallback");
	record_route_name(route_names, add_typed_route<typed_summary_route>(routes), "summary");
	record_route_name(route_names, add_typed_route<typed_projection_route>(routes), "projection");
	record_route_name(route_names, add_typed_route<typed_summary_projection_route>(routes), "summary_projection");

	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?summary=true", 11)),
	          "summary");
	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?fields=name", 11)),
	          "projection");
	EXPECT_EQ(
	    matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?summary=true&fields=name", 11)),
	    "summary_projection");
	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?unused=1", 11)),
	          "fallback");
}

TEST(RegistryTest, AddCompiledPrefersExactValueConstraintsOverBroadMatches) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_typed_route<typed_broad_mode_route>(routes), "broad");
	record_route_name(route_names, add_typed_route<typed_exact_mode_route>(routes), "exact");

	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/items?mode=full", 11)), "exact");
	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/items?mode=compact", 11)), "broad");
}

TEST(RegistryTest, AddCompiledRejectsTypedDuplicatesWithReorderedConstraints) {
	registry routes;

	add_typed_route<typed_summary_fields_route>(routes);
	EXPECT_THROW(add_typed_route<typed_fields_summary_route>(routes), std::invalid_argument);
}

TEST(RegistryTest, AddCompiledRejectsDuplicatesAgainstStringRoutes) {
	registry routes;

	add_route(routes, warp::method::get, "/reports/{id}?summary&fields");
	EXPECT_THROW(add_typed_route<typed_fields_summary_route>(routes), std::invalid_argument);
}

TEST(RegistryTest, AddTypedSupportsEncodedExactValueConstraints) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_typed_route<typed_encoded_query_route>(routes), "encoded");

	EXPECT_EQ(
	    matched_route_name(routes, route_names, warp::request(verb::get, "/filters?plus%2Bspace%20%25=a%2Bb+%25", 11)),
	    "encoded");
	EXPECT_TRUE(
	    matched_route_name(routes, route_names, warp::request(verb::get, "/filters?plus%2Bspace%20%25=other", 11))
	        .empty());
}

TEST(RegistryTest, FindSupportsNegativeQueryConstraints) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary&!fields"),
	                  "summary_without_fields");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary&fields"),
	                  "summary_with_fields");

	EXPECT_EQ(matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?summary=true", 11)),
	          "summary_without_fields");
	EXPECT_EQ(
	    matched_route_name(routes, route_names, warp::request(verb::get, "/reports/42?summary=true&fields=name", 11)),
	    "summary_with_fields");
}

TEST(RegistryTest, FindDoesNotTreatDuplicateQueriesAsSatisfiedSpecificMatchers) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}"), "fallback");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?summary"), "summary");

	warp::request req(verb::get, "/reports/42?summary=true&summary=false", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	ASSERT_TRUE(req.target_error().has_value());
	EXPECT_EQ(req.target_error()->code, "duplicate_query_parameter");
	EXPECT_EQ(route_names.at(match->id.index()), "fallback");
}

TEST(RegistryTest, FindDoesNotTreatMalformedQueriesAsSatisfiedSpecificMatchers) {
	registry routes;
	std::vector<std::string> route_names;
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}"), "fallback");
	record_route_name(route_names, add_route(routes, warp::method::get, "/reports/{id}?fields"), "projection");

	warp::request req(verb::get, "/reports/42?fields=%ZZ", 11);
	const auto match = routes.find(req);
	ASSERT_TRUE(match.has_value());
	ASSERT_TRUE(req.target_error().has_value());
	EXPECT_EQ(req.target_error()->code, "malformed_query_parameter");
	EXPECT_EQ(route_names.at(match->id.index()), "fallback");
}

TEST(RegistryTest, FindReturnsNullForUnknownMethodOrPath) {
	registry routes;
	add_route(routes, warp::method::get, "/users/{id}");

	warp::request wrong_method(verb::post, "/users/42", 11);
	warp::request wrong_path(verb::get, "/accounts/42", 11);

	EXPECT_EQ(routes.find(wrong_method), std::nullopt);
	EXPECT_EQ(routes.find(wrong_path), std::nullopt);
}

TEST(RegistryTest, FindClearsStalePathParamsOnMiss) {
	registry routes;
	add_route(routes, warp::method::get, "/users/{id}");

	warp::request req(verb::get, "/users/42", 11);
	ASSERT_TRUE(routes.find(req).has_value());
	ASSERT_TRUE(req.path_param(std::string_view {"id"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"id"}), "42");

	req.target("/accounts/42");
	req.refresh_target_metadata();
	EXPECT_EQ(routes.find(req), std::nullopt);
	EXPECT_FALSE(req.path_param(std::string_view {"id"}).has_value());
}

TEST(RegistryTest, AddRejectsInvalidRoutePatterns) {
	registry routes;

	EXPECT_THROW(static_cast<void>(add_route(routes, warp::method::get, "users/{id}")), std::invalid_argument);
	EXPECT_THROW(static_cast<void>(add_route(routes, warp::method::get, "/users//id")), std::invalid_argument);
	EXPECT_THROW(static_cast<void>(add_route(routes, warp::method::get, "/users/{}")), std::invalid_argument);
	EXPECT_THROW(static_cast<void>(add_route(routes, warp::method::get, "/reports/{id}?summary&summary")),
	             std::invalid_argument);
}

TEST(RegistryTest, AddRejectsDuplicateNormalizedRouteShapesPerMethod) {
	registry routes;

	add_route(routes, warp::method::get, "/users/{id}");
	EXPECT_THROW(static_cast<void>(add_route(routes, warp::method::get, "/users/{name}")), std::invalid_argument);
	add_route(routes, warp::method::post, "/users/{name}");

	add_route(routes, warp::method::get, "/reports/{id}?summary");
	EXPECT_THROW(static_cast<void>(add_route(routes, warp::method::get, "/reports/{report_id}?summary")),
	             std::invalid_argument);
	add_route(routes, warp::method::get, "/reports/{report_id}?fields");
}

TEST(RegistryTest, CopyConstructorPreservesRouteTree) {
	registry original;
	const auto original_id = add_route(original, warp::method::get, "/copy/{id}");

	registry copied = original;
	warp::request req(verb::get, "/copy/77", 11);
	const auto match = copied.find(req);
	ASSERT_TRUE(match.has_value());
	EXPECT_EQ(match->id, original_id);
	ASSERT_TRUE(req.path_param(std::string_view {"id"}).has_value());
	EXPECT_EQ(*req.path_param(std::string_view {"id"}), "77");
}

} // namespace
