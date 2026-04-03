#include "warp/codegen/generator.hpp"
#include "warp/codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using warp::codegen::api_stub_generator;
using warp::codegen::parse_api_spec;

TEST(ApiStubGeneratorTest, GeneratesModelAndResourceHeadersWithConsistentNames) {
	const auto spec = parse_api_spec(R"(
cpp_namespace: generated_api
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
            - name: x-trace-id
              in: header
              type: string
          body:
            type: object
            fields:
              - name: name
                type: string
        response:
          status: 201
          body:
            type: object
            fields:
              - name: id
                type: int64
)");

	const auto generated =
	    api_stub_generator().generate(spec, {.namespace_name = "generated_api", .model_header_name = "models.hpp"});

	EXPECT_NE(generated.model_header.find("namespace generated_api {"), std::string::npos);
	EXPECT_NE(generated.model_header.find("struct users_create_user_request"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("#include \"models.hpp\""), std::string::npos);
	EXPECT_NE(generated.resource_header.find("class users_api_base"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("parse_http_request<generated_api::users_create_user_request>"),
	          std::string::npos);
	EXPECT_NE(generated.resource_header.find("derived().create_user(std::move(typed_request));"), std::string::npos);
}

} // namespace
