#include "codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>

namespace {

using warp::codegen::http_method;
using warp::codegen::parameter_location;
using warp::codegen::parse_api_spec;
using warp::codegen::spec_error;
using warp::codegen::value_kind;

template <typename Fn>
warp::codegen::diagnostic capture_spec_diagnostic(Fn &&fn) {
	try {
		fn();
		ADD_FAILURE() << "expected spec_error";
		return {};
	} catch (const spec_error &error) {
		return error.item();
	}
}

TEST(SpecParserTest, ParsesNestedResourcesEndpointsAndSchemas) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: create_user
        method: POST
        path: /users/{user_id}
        request:
          parameters:
            - name: user_id
              in: path
              type: string
            - name: include_deleted
              in: query
              type: bool
              required: false
          body:
            type: object
            fields:
              - name: profile
                type: object
                fields:
                  - name: email
                    type: string
                  - name: tags
                    type: array
                    items:
                      type: string
        response:
          status: 201
          body:
            type: object
            fields:
              - name: id
                type: int64
)");

	ASSERT_EQ(spec.resources.size(), 1U);
	const auto &resource = spec.resources.front();
	EXPECT_EQ(resource.name, "users");
	ASSERT_EQ(resource.endpoints.size(), 1U);

	const auto &endpoint = resource.endpoints.front();
	EXPECT_EQ(endpoint.name, "create_user");
	EXPECT_EQ(endpoint.method, http_method::post);
	EXPECT_EQ(endpoint.path, "/users/{user_id}");
	ASSERT_EQ(endpoint.request.parameters.size(), 2U);
	EXPECT_EQ(endpoint.request.parameters[0].name, "user_id");
	EXPECT_EQ(endpoint.request.parameters[0].location, parameter_location::path);
	EXPECT_EQ(endpoint.request.parameters[1].name, "include_deleted");
	EXPECT_EQ(endpoint.request.parameters[1].kind, value_kind::bool_value);
	EXPECT_FALSE(endpoint.request.parameters[1].required);
	ASSERT_TRUE(endpoint.request.body.has_value());
	EXPECT_EQ(endpoint.request.body->kind, value_kind::object_value);
	ASSERT_EQ(endpoint.request.body->fields.size(), 1U);
	EXPECT_EQ(endpoint.request.body->fields[0].name, "profile");
	ASSERT_NE(endpoint.request.body->fields[0].value, nullptr);
	EXPECT_EQ(endpoint.request.body->fields[0].value->kind, value_kind::object_value);
	ASSERT_EQ(endpoint.request.body->fields[0].value->fields.size(), 2U);
	EXPECT_EQ(endpoint.request.body->fields[0].value->fields[1].value->kind, value_kind::array_value);
	EXPECT_EQ(endpoint.request.body->fields[0].value->fields[1].value->element_type->kind, value_kind::string_value);
	EXPECT_EQ(endpoint.response.status_code, 201);
	ASSERT_TRUE(endpoint.response.body.has_value());
	EXPECT_EQ(endpoint.response.body->fields[0].value->kind, value_kind::int64_value);
}

TEST(SpecParserTest, AppliesTopLevelEndpointGroupingAndDefaults) {
	const auto spec = parse_api_spec(R"(
name: diagnostics
endpoints:
  - path: /health
    response:
      type: object
      fields:
        - name: ok
          type: bool
)");

	ASSERT_EQ(spec.resources.size(), 1U);
	EXPECT_EQ(spec.resources.front().name, "diagnostics");
	ASSERT_EQ(spec.resources.front().endpoints.size(), 1U);
	const auto &endpoint = spec.resources.front().endpoints.front();
	EXPECT_EQ(endpoint.method, http_method::get);
	EXPECT_EQ(endpoint.name, "get_health");
	EXPECT_EQ(endpoint.response.status_code, 200);
	ASSERT_TRUE(endpoint.response.body.has_value());
	EXPECT_EQ(endpoint.response.body->fields[0].name, "ok");
}

TEST(SpecParserTest, RejectsMissingRequiredKeys) {
	EXPECT_THROW(static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - method: GET
        response:
          status: 200
)")),
	             spec_error);
}

TEST(SpecParserTest, ReportsInvalidIndentationWithLocation) {
	try {
		static_cast<void>(parse_api_spec("resources:\n\t- name: users\n"));
		FAIL() << "expected spec_error";
	} catch (const spec_error &error) {
		EXPECT_EQ(error.line(), 2U);
		EXPECT_EQ(error.column(), 1U);
	}
}

TEST(SpecParserTest, RejectsUnsupportedScalarKinds) {
	EXPECT_THROW(static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - path: /users
        response:
          body:
            type: uuid
)")),
	             spec_error);
}

TEST(SpecParserTest, RejectsDuplicateKeys) {
	EXPECT_THROW(static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    name: duplicate
    endpoints:
      - path: /health
)")),
	             spec_error);
}

TEST(SpecParserTest, RejectsUnknownKeys) {
	EXPECT_THROW(static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - path: /health
        typo_key: true
)")),
	             spec_error);
}

TEST(SpecParserTest, RejectsInvalidStatusStrings) {
	EXPECT_THROW(static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - path: /health
        response:
          status: ok
)")),
	             spec_error);
}

TEST(SpecParserTest, ReportsDuplicateKeysWithStableDiagnostics) {
	const auto item = capture_spec_diagnostic([&] {
		static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    name: duplicate
    endpoints:
      - path: /health
)"));
	});

	EXPECT_EQ(item.code, "spec.duplicate_key");
	EXPECT_EQ(item.span.line, 4U);
	EXPECT_NE(item.message.find("duplicate key 'name'"), std::string::npos);
}

TEST(SpecParserTest, ReportsUnknownKeysWithStableDiagnostics) {
	const auto item = capture_spec_diagnostic([&] {
		static_cast<void>(parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - path: /health
        typo_key: true
)"));
	});

	EXPECT_EQ(item.code, "spec.unknown_key");
	EXPECT_EQ(item.span.line, 6U);
	EXPECT_NE(item.message.find("unknown key 'typo_key'"), std::string::npos);
}

TEST(SpecParserTest, ParsesValidationRulesOnParametersAndFields) {
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

	ASSERT_EQ(spec.resources.size(), 1U);
	const auto &endpoint = spec.resources.front().endpoints.front();
	ASSERT_EQ(endpoint.request.parameters.size(), 1U);
	EXPECT_FALSE(endpoint.request.parameters.front().required);
	ASSERT_TRUE(endpoint.request.parameters.front().validation.min.has_value());
	EXPECT_EQ(std::get<std::int64_t>(*endpoint.request.parameters.front().validation.min), 1);
	ASSERT_TRUE(endpoint.request.parameters.front().validation.max.has_value());
	EXPECT_EQ(std::get<std::int64_t>(*endpoint.request.parameters.front().validation.max), 50);
	EXPECT_EQ(endpoint.request.parameters.front().validation.min_span.line, 14U);
	EXPECT_EQ(endpoint.request.parameters.front().validation.max_span.line, 15U);

	ASSERT_TRUE(endpoint.request.body.has_value());
	ASSERT_EQ(endpoint.request.body->fields.size(), 2U);
	const auto &display_name = endpoint.request.body->fields[0];
	ASSERT_NE(display_name.value, nullptr);
	EXPECT_FALSE(display_name.required);
	ASSERT_TRUE(display_name.value->validation.min_length.has_value());
	ASSERT_TRUE(display_name.value->validation.max_length.has_value());
	EXPECT_EQ(*display_name.value->validation.min_length, 3U);
	EXPECT_EQ(*display_name.value->validation.max_length, 32U);
	EXPECT_EQ(display_name.value->validation.min_length_span.line, 22U);
	EXPECT_EQ(display_name.value->validation.max_length_span.line, 23U);

	const auto &rating = endpoint.request.body->fields[1];
	ASSERT_NE(rating.value, nullptr);
	ASSERT_TRUE(rating.value->validation.min.has_value());
	ASSERT_TRUE(rating.value->validation.max.has_value());
	EXPECT_DOUBLE_EQ(std::get<double>(*rating.value->validation.min), 0.5);
	EXPECT_DOUBLE_EQ(std::get<double>(*rating.value->validation.max), 5.0);
}

TEST(SpecParserTest, RejectsInvalidValidationRuleLiterals) {
	const auto item = capture_spec_diagnostic([&] {
		static_cast<void>(parse_api_spec(R"(
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
              min: first
        response:
          status: 204
)"));
	});

	EXPECT_EQ(item.code, "spec.parse");
	EXPECT_EQ(item.span.line, 13U);
	EXPECT_NE(item.message.find("validation rule 'min'"), std::string::npos);
}

} // namespace
