#include "warp/codegen/model.hpp"
#include "warp/codegen/spec_parser.hpp"

#include <gtest/gtest.h>

namespace {

using warp::codegen::build_api_model;
using warp::codegen::diagnostic;
using warp::codegen::diagnostic_error;
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

} // namespace
