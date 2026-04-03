#include "warp/codegen/model.hpp"
#include "warp/codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using warp::codegen::build_api_model;
using warp::codegen::parse_api_spec;

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

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), std::invalid_argument);
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

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), std::invalid_argument);
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

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), std::invalid_argument);
}

TEST(ApiModelTest, RejectsConflictingExplicitSchemaNames) {
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
            name: SharedPayload
            fields:
              - name: name
                type: string
)");

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), std::invalid_argument);
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

	EXPECT_THROW(static_cast<void>(build_api_model(spec)), std::invalid_argument);
}

} // namespace
