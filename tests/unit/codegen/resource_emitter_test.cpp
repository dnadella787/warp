#include "warp/codegen/resource_emitter.hpp"
#include "warp/codegen/model.hpp"
#include "warp/codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using warp::codegen::build_api_model;
using warp::codegen::parse_api_spec;
using warp::codegen::resource_emitter;

TEST(ResourceEmitterTest, EmitsTraitsAndSharedOwnershipRouteAdapters) {
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
            - name: verbose
              in: query
              type: bool
              required: false
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
      - name: health
        path: /health
        response:
          status: 204
)");

	const auto output =
	    resource_emitter().emit_header(build_api_model(spec, "generated_api"),
	                                   {.model_header_include = "generated_models.hpp", .include_model_header = true});

	EXPECT_NE(output.find("#include \"generated_models.hpp\""), std::string::npos);
	EXPECT_NE(output.find("struct request_contract_traits<generated_api::users_create_user_request>"),
	          std::string::npos);
	EXPECT_NE(output.find("required_path_param<std::string>(req, \"user_id\")"), std::string::npos);
	EXPECT_NE(output.find("optional_query_param<bool>(req, \"verbose\")"), std::string::npos);
	EXPECT_NE(output.find("required_header_param<std::string>(req, \"x-trace-id\")"), std::string::npos);
	EXPECT_NE(output.find("json_body<generated_api::users_create_user_request_body>(req)"), std::string::npos);
	EXPECT_NE(output.find("struct response_contract_traits<generated_api::users_health_response>"), std::string::npos);
	EXPECT_NE(output.find("static constexpr bool has_body = false;"), std::string::npos);
	EXPECT_NE(output.find("class users_api_routes"), std::string::npos);
	EXPECT_NE(output.find("std::shared_ptr<Service> service_"), std::string::npos);
	EXPECT_NE(output.find("service->create_user(std::move(typed_request));"), std::string::npos);
}

TEST(ResourceEmitterTest, EscapesSpecDerivedStringLiteralsInGeneratedOutput) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: users
    endpoints:
      - name: fetch_user
        path: '/users/"quoted"'
        request:
          parameters:
            - name: 'x-"trace"'
              in: header
              type: string
        response:
          status: 204
)");

	const auto output = resource_emitter().emit_header(build_api_model(spec, "generated_api"));

	EXPECT_NE(output.find(R"(builder.route(warp::method::get, "/users/\"quoted\"")"), std::string::npos);
	EXPECT_NE(output.find(R"(required_header_param<std::string>(req, "x-\"trace\""))"), std::string::npos);
}

} // namespace
