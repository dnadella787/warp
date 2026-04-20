#include "warp/codegen/generator.hpp"
#include "warp/codegen/model.hpp"
#include "warp/codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
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
	const auto generated_again =
	    api_stub_generator().generate(spec, {.namespace_name = "generated_api", .model_header_name = "models.hpp"});

	EXPECT_NE(generated.model_header.find("namespace generated_api {"), std::string::npos);
	EXPECT_NE(generated.model_header.find("struct users_create_user_request"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("#include \"models.hpp\""), std::string::npos);
	EXPECT_NE(generated.resource_header.find("using users_api_routes = warp::codegen::generated_resource<"),
	          std::string::npos);
	EXPECT_NE(generated.resource_header.find("namespace generated_api::codegen_detail {"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("using users_create_user_request_contract = "
	                                         "warp::codegen::generated_request_contract<"
	                                         "users_create_user_request"),
	          std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("struct request_contract_traits<generated_api::users_create_user_request> : "
	                                   "generated_api::codegen_detail::users_create_user_request_contract {};"),
	    std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("[](Service &service, generated_api::users_create_user_request &&typed_request) "
	                                   "-> decltype(auto) {"),
	    std::string::npos);
	EXPECT_NE(generated.resource_header.find("struct users_create_user_request_handler_selector {"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("return warp::codegen::invoke_endpoint_handler_overload<"),
	          std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("generated_api::codegen_detail::users_create_user_request_handler_selector>("),
	    std::string::npos);
	EXPECT_EQ(generated.model_header, generated_again.model_header);
	EXPECT_EQ(generated.resource_header, generated_again.resource_header);
}

TEST(ApiStubGeneratorTest, RejectsNamespaceOverrideMismatchForModels) {
	const auto spec = parse_api_spec(R"(
cpp_namespace: generated_api
resources:
  - name: users
    endpoints:
      - path: /health
        response:
          status: 204
)");

	const auto model = warp::codegen::build_api_model(spec);
	EXPECT_THROW(static_cast<void>(api_stub_generator().generate(model, {.namespace_name = "other_namespace"})),
	             std::invalid_argument);
}

} // namespace
