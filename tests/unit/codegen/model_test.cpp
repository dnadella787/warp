#include "codegen/model.hpp"
#include "codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <variant>

namespace {

using warp::codegen::build_api_model;
using warp::codegen::diagnostic;
using warp::codegen::diagnostic_error;
using warp::codegen::object_schema_model;
using warp::codegen::parse_api_spec;

template <typename Fn>
diagnostic capture_diagnostic(Fn &&fn) {
	try {
		fn();
		ADD_FAILURE() << "expected diagnostic_error";
		return {};
	} catch (const diagnostic_error &error) {
		return error.item();
	}
}

const object_schema_model *find_schema(const warp::codegen::api_model &model, const std::string &name) {
	for (const auto &schema : model.schemas) {
		if (schema.name == name) {
			return &schema;
		}
	}
	return nullptr;
}

TEST(ApiModelTest, RejectsMissingPathParameterDeclaration) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: fetch_user
        path: /users/{user_id}
        response:
          status: 204
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, RejectsOptionalPathParameters) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: fetch_user
        path: /users/{user_id}
        request:
          parameters:
            - name: user_id
              in: path
              type: string
              required: false
        response:
          status: 204
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, RejectsDeclaredPathParametersNotPresentInRoute) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: fetch_user
        path: /users
        request:
          parameters:
            - name: user_id
              in: path
              type: string
        response:
          status: 204
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, RejectsSchemaTypeNameSymbolCollisions) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: create_user
        path: /users
        request:
          body:
            type: object
            name: SharedPayload
            fields:
              - name: id
                type: int64
        response:
          body:
            type: object
            name: shared_payload
            fields:
              - name: name
                type: string
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, RejectsNullableSchemas) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: create_user
        path: /users
        request:
          body:
            type: object
            fields:
              - name: nickname
                type: string
                nullable: true
        response:
          status: 204
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, RejectsDuplicateNormalizedRouteShapes) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: fetch_user_by_id
        method: GET
        path: /users/{id}
        request:
          parameters:
            - name: id
              in: path
              type: string
      - name: fetch_user_by_name
        method: GET
        path: /users/{name}
        request:
          parameters:
            - name: name
              in: path
              type: string
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, AcceptsDistinctOverlappingQueryRouteSets) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: reports
    endpoints:
      - name: fetch_report
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
        response:
          status: 204
      - name: fetch_report_summary
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
        response:
          status: 204
      - name: fetch_report_projection
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: fields
              in: query
              type: string
              required: true
        response:
          status: 204
      - name: fetch_report_summary_projection
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
            - name: fields
              in: query
              type: string
              required: true
        response:
          status: 204
)");

	const auto model = build_api_model(spec);
	ASSERT_EQ(model.resources.size(), 1U);
	const auto &resource = model.resources.front();
	ASSERT_EQ(resource.endpoints.size(), 4U);
	ASSERT_EQ(resource.route_groups.size(), 1U);
	EXPECT_EQ(resource.route_groups.front().routing_query_parameters, (std::vector<std::string> {"summary", "fields"}));

	ASSERT_TRUE(resource.endpoints[1].query_route.has_value());
	ASSERT_EQ(resource.endpoints[1].query_route->constraints.size(), 1U);
	EXPECT_EQ(resource.endpoints[1].query_route->constraints.front().name, "summary");
	EXPECT_EQ(resource.endpoints[1].query_route->constraints.front().presence,
	          warp::http::query_constraint_presence::required);

	ASSERT_TRUE(resource.endpoints[2].query_route.has_value());
	ASSERT_EQ(resource.endpoints[2].query_route->constraints.size(), 1U);
	EXPECT_EQ(resource.endpoints[2].query_route->constraints.front().name, "fields");
	EXPECT_EQ(resource.endpoints[2].query_route->constraints.front().presence,
	          warp::http::query_constraint_presence::required);

	ASSERT_TRUE(resource.endpoints[3].query_route.has_value());
	ASSERT_EQ(resource.endpoints[3].query_route->constraints.size(), 2U);
	EXPECT_EQ(resource.endpoints[3].query_route->constraints.front().name, "fields");
	EXPECT_EQ(resource.endpoints[3].query_route->constraints.front().presence,
	          warp::http::query_constraint_presence::required);
	EXPECT_EQ(resource.endpoints[3].query_route->constraints.back().name, "summary");
	EXPECT_EQ(resource.endpoints[3].query_route->constraints.back().presence,
	          warp::http::query_constraint_presence::required);
}

TEST(ApiModelTest, AcceptsDeterministicRequiredAndOptionalQueryRouteSets) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: reports
    endpoints:
      - name: fetch_report_summary
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
        response:
          status: 204
      - name: fetch_report_summary_with_optional_fields
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
            - name: fields
              in: query
              type: string
              required: false
        response:
          status: 204
)");

	const auto model = build_api_model(spec);
	ASSERT_EQ(model.resources.size(), 1U);
	ASSERT_EQ(model.resources.front().endpoints.size(), 2U);
	ASSERT_EQ(model.resources.front().route_groups.size(), 1U);
	EXPECT_EQ(model.resources.front().route_groups.front().routing_query_parameters,
	          (std::vector<std::string> {"summary", "fields"}));
}

TEST(ApiModelTest, RejectsQueryRoutesThatRemainAmbiguousAfterForbiddenExpansion) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: reports
    endpoints:
      - name: fetch_report_summary_with_optional_fields
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
            - name: fields
              in: query
              type: string
              required: false
        response:
          status: 204
      - name: fetch_report_summary_with_optional_locale
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
            - name: locale
              in: query
              type: string
              required: false
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.ambiguous_query_route");
	EXPECT_NE(item.message.find("query-aware routes"), std::string::npos);
}

TEST(ApiModelTest, LeavesSingletonRequiredQueryEndpointsUnconstrained) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: reports
    endpoints:
      - name: fetch_report_summary
        method: GET
        path: /reports/{report_id}
        request:
          parameters:
            - name: report_id
              in: path
              type: string
              required: true
            - name: summary
              in: query
              type: bool
              required: true
        response:
          status: 204
)");

	const auto model = build_api_model(spec);

	ASSERT_EQ(model.resources.size(), 1U);
	const auto &resource = model.resources.front();
	ASSERT_EQ(resource.endpoints.size(), 1U);
	ASSERT_EQ(resource.route_groups.size(), 1U);
	EXPECT_FALSE(resource.endpoints.front().query_route.has_value());
	EXPECT_TRUE(resource.route_groups.front().query_route_endpoint_indices.empty());
	EXPECT_TRUE(resource.route_groups.front().routing_query_parameters.empty());
}

TEST(ApiModelTest, RejectsResponseBodyForNoContentStatuses) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: health
        path: /health
        response:
          status: "204 No Content"
          body:
            type: object
            fields:
              - name: ok
                type: bool
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), diagnostic_error);
}

TEST(ApiModelTest, RejectsRequestBodiesForGetEndpointsWithStructuredDiagnostics) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: fetch_user
        method: GET
        path: /users/{id}
        request:
          parameters:
            - name: id
              in: path
              type: string
          body:
            type: object
            fields:
              - name: include_deleted
                type: bool
        response:
          status: 200
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.request_body_forbidden");
	EXPECT_GT(item.span.line, 0U);
	EXPECT_NE(item.message.find("GET"), std::string::npos);
}

TEST(ApiModelTest, RejectsInvalidNamespacesWithStructuredDiagnostics) {
	const auto spec = parse_api_spec(R"(
cpp_namespace: bad-namespace
resources:
  - name: users
    endpoints:
      - path: /health
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.invalid_namespace");
	EXPECT_GT(item.span.line, 0U);
	EXPECT_NE(item.message.find("namespace"), std::string::npos);
}

TEST(ApiModelTest, RejectsInvalidNamespaceOverridesWithStructuredDiagnostics) {
	const auto spec = parse_api_spec(R"(
cpp_namespace: generated_api
resources:
  - name: users
    endpoints:
      - path: /health
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec, "invalid-namespace")); });
	EXPECT_EQ(item.code, "model.invalid_namespace");
	EXPECT_NE(item.message.find("valid identifiers"), std::string::npos);
}

TEST(ApiModelTest, NormalizesValidationRulesOnOptionalParametersAndFields) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: create_user
        method: POST
        path: /users
        request:
          parameters:
            - name: page
              in: query
              type: int64
              required: false
              min: 1
              max: 50
            - name: search
              in: query
              type: string
              required: false
              min_length: 2
              max_length: 64
          body:
            type: object
            fields:
              - name: display_name
                type: string
                required: false
                min_length: 3
                max_length: 32
              - name: rating
                type: double
                min: 0.5
                max: 5.0
        response:
          status: 204
)");

	const auto model = build_api_model(spec);
	ASSERT_EQ(model.resources.size(), 1U);
	const auto &endpoint = model.resources.front().endpoints.front();
	ASSERT_EQ(endpoint.request.parameters.size(), 2U);

	const auto &page = endpoint.request.parameters[0];
	EXPECT_FALSE(page.required);
	ASSERT_TRUE(page.validation.min.has_value());
	ASSERT_TRUE(page.validation.max.has_value());
	EXPECT_EQ(std::get<std::int64_t>(*page.validation.min), 1);
	EXPECT_EQ(std::get<std::int64_t>(*page.validation.max), 50);

	const auto &search = endpoint.request.parameters[1];
	EXPECT_FALSE(search.required);
	EXPECT_EQ(search.source_name, "search");
	ASSERT_TRUE(search.validation.min_length.has_value());
	ASSERT_TRUE(search.validation.max_length.has_value());
	EXPECT_EQ(*search.validation.min_length, 2U);
	EXPECT_EQ(*search.validation.max_length, 64U);

	ASSERT_TRUE(endpoint.request.body_type_name.has_value());
	const auto *schema = find_schema(model, *endpoint.request.body_type_name);
	ASSERT_NE(schema, nullptr);
	ASSERT_EQ(schema->fields.size(), 2U);

	const auto &display_name = schema->fields[0];
	EXPECT_FALSE(display_name.required);
	ASSERT_TRUE(display_name.validation.min_length.has_value());
	ASSERT_TRUE(display_name.validation.max_length.has_value());
	EXPECT_EQ(*display_name.validation.min_length, 3U);
	EXPECT_EQ(*display_name.validation.max_length, 32U);

	const auto &rating = schema->fields[1];
	ASSERT_TRUE(rating.validation.min.has_value());
	ASSERT_TRUE(rating.validation.max.has_value());
	EXPECT_DOUBLE_EQ(std::get<double>(*rating.validation.min), 0.5);
	EXPECT_DOUBLE_EQ(std::get<double>(*rating.validation.max), 5.0);
}

TEST(ApiModelTest, RejectsInvalidParameterValidationRuleCombinations) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: list_users
        method: GET
        path: /users
        request:
          parameters:
            - name: search
              in: query
              type: string
              min: 1
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.invalid_validation_rule");
	EXPECT_EQ(item.span.line, 13U);
	EXPECT_NE(item.message.find("parameter 'search'"), std::string::npos);
}

TEST(ApiModelTest, RejectsInvalidFieldValidationRuleCombinations) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: create_user
        method: POST
        path: /users
        request:
          body:
            type: object
            fields:
              - name: age
                type: int64
                min_length: 1
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.invalid_validation_rule");
	EXPECT_EQ(item.span.line, 14U);
	EXPECT_NE(item.message.find("field 'age'"), std::string::npos);
}

TEST(ApiModelTest, RejectsInvertedNumericValidationRanges) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: list_users
        method: GET
        path: /users
        request:
          parameters:
            - name: page
              in: query
              type: int64
              min: 10
              max: 2
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.invalid_validation_range");
	EXPECT_EQ(item.span.line, 14U);
	EXPECT_NE(item.message.find("parameter 'page'"), std::string::npos);
}

TEST(ApiModelTest, RejectsInvertedStringValidationRanges) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: create_user
        method: POST
        path: /users
        request:
          body:
            type: object
            fields:
              - name: display_name
                type: string
                min_length: 10
                max_length: 2
        response:
          status: 204
)");

	const auto item = capture_diagnostic([&] { static_cast<void>(build_api_model(spec)); });
	EXPECT_EQ(item.code, "model.invalid_validation_range");
	EXPECT_EQ(item.span.line, 15U);
	EXPECT_NE(item.message.find("field 'display_name'"), std::string::npos);
}

} // namespace
