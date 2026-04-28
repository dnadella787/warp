#include "warp/codegen/generator.hpp"
#include "codegen/model.hpp"
#include "codegen/spec_parser.hpp"
#include "codegen/stub_generator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

using warp::codegen::api_stub_generator;
TEST(ApiStubGeneratorTest, GeneratesModelAndResourceHeadersWithConsistentNames) {
	static constexpr std::string_view yaml = R"(
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
)";

	const auto generated = api_stub_generator().generate_from_yaml(
	    yaml, {.namespace_name = "generated_api", .model_header_name = "models.hpp"});
	const auto generated_again = api_stub_generator().generate_from_yaml(
	    yaml, {.namespace_name = "generated_api", .model_header_name = "models.hpp"});

	EXPECT_NE(generated.model_header.find("namespace generated_api {"), std::string::npos);
	EXPECT_NE(generated.model_header.find("struct users_create_user_request {"), std::string::npos);
	EXPECT_NE(generated.model_header.find("#include \"warp/codegen/json_object_contract.hpp\""), std::string::npos);
	EXPECT_NE(generated.model_header.find("struct json_object_contract<generated_api::users_create_user_request_body>"),
	          std::string::npos);
	EXPECT_NE(generated.model_header.find("&generated_api::users_create_user_request_body::name"), std::string::npos);
	EXPECT_EQ(generated.model_header.find("class Builder {"), std::string::npos);
	EXPECT_EQ(generated.model_header.find("set_user_id("), std::string::npos);
	EXPECT_NE(generated.resource_header.find("#include \"models.hpp\""), std::string::npos);
	EXPECT_NE(generated.resource_header.find("using users_api_routes = warp::codegen::generated_resource<"),
	          std::string::npos);
	EXPECT_NE(generated.resource_header.find("namespace generated_api::codegen_detail {"), std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("struct request_contract_traits<generated_api::users_create_user_request> : "
	                                   "warp::codegen::generated_request_contract<"),
	    std::string::npos);
	EXPECT_EQ(generated.resource_header.find("struct users_create_user_request_validator {"), std::string::npos);
	EXPECT_EQ(generated.resource_header.find("struct users_create_user_request_body_validator {"), std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("struct response_contract_traits<generated_api::users_create_user_response> {"),
	    std::string::npos);
	EXPECT_NE(generated.resource_header.find("static decltype(auto) body(response_type &&value) {"), std::string::npos);
	EXPECT_NE(generated.resource_header.find(
	              "warp::codegen::path_field_binding<&generated_api::users_create_user_request::user_id, "
	              "\"user_id\">"),
	          std::string::npos);
	EXPECT_NE(generated.resource_header.find(
	              "warp::codegen::header_field_binding<&generated_api::users_create_user_request::x_trace_id, "
	              "\"x-trace-id\">"),
	          std::string::npos);
	EXPECT_NE(generated.resource_header.find(
	              "warp::codegen::json_body_field_binding<&generated_api::users_create_user_request::body>"),
	          std::string::npos);
	EXPECT_NE(generated.resource_header.find("return (value.body);"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("return (std::move(value).body);"), std::string::npos);
	EXPECT_NE(generated.resource_header.find("using users_create_user_request_endpoint = "
	                                         "warp::codegen::generated_endpoint_binding<"),
	          std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("using users_create_user_request_handler_result = "
	                                   "warp::codegen::handler_result<generated_api::users_create_user_response>;"),
	    std::string::npos);
	EXPECT_NE(generated.resource_header.find("struct users_create_user_request_handler_selector {"), std::string::npos);
	EXPECT_NE(
	    generated.resource_header.find("generated_api::codegen_detail::users_create_user_request_handler_selector>;"),
	    std::string::npos);
	EXPECT_EQ(generated.resource_header.find("struct users_create_user_request_user_id_accessor {"), std::string::npos);
	EXPECT_EQ(generated.resource_header.find("using users_create_user_request_contract ="), std::string::npos);
	EXPECT_EQ(
	    generated.resource_header.find("[](Service &service, generated_api::users_create_user_request &&typed_request) "
	                                   "-> decltype(auto) {"),
	    std::string::npos);
	EXPECT_EQ(generated.model_header, generated_again.model_header);
	EXPECT_EQ(generated.resource_header, generated_again.resource_header);
}

TEST(ApiStubGeneratorTest, RejectsNamespaceOverrideMismatchForModels) {
	const auto spec = warp::codegen::parse_api_spec(R"(
cpp_namespace: generated_api
resources:
  - name: users
    endpoints:
      - path: /health
        response:
          status: 204
)");

	const auto model = warp::codegen::build_api_model(spec);
	EXPECT_THROW(
	    static_cast<void>(warp::codegen::stub_generator().generate(model, {.namespace_name = "other_namespace"})),
	    std::invalid_argument);
}

} // namespace
