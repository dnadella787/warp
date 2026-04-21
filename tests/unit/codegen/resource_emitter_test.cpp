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
	EXPECT_NE(output.find("namespace generated_api::codegen_detail {"), std::string::npos);
	EXPECT_NE(output.find("struct request_contract_traits<generated_api::users_create_user_request> : "
	                      "warp::codegen::generated_request_contract<"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::path_setter_binding<&generated_api::users_create_user_request::set_user_id, "
	                      "\"user_id\">"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::query_setter_binding<&generated_api::users_create_user_request::set_verbose, "
	                      "\"verbose\">"),
	          std::string::npos);
	EXPECT_NE(
	    output.find("warp::codegen::header_setter_binding<&generated_api::users_create_user_request::set_x_trace_id, "
	                "\"x-trace-id\">"),
	    std::string::npos);
	EXPECT_NE(
	    output.find("warp::codegen::json_body_setter_binding<&generated_api::users_create_user_request::set_body>"),
	    std::string::npos);
	EXPECT_NE(output.find("struct response_contract_traits<generated_api::users_create_user_response> {"),
	          std::string::npos);
	EXPECT_NE(output.find("using response_type = generated_api::users_create_user_response;"), std::string::npos);
	EXPECT_NE(output.find("static constexpr unsigned status_code = response_type::status_code;"), std::string::npos);
	EXPECT_NE(output.find("static constexpr bool has_body = true;"), std::string::npos);
	EXPECT_NE(output.find("static decltype(auto) body(const response_type &value) {"), std::string::npos);
	EXPECT_NE(output.find("return value.body();"), std::string::npos);
	EXPECT_NE(output.find("static decltype(auto) body(response_type &&value) {"), std::string::npos);
	EXPECT_NE(output.find("return std::move(value).body();"), std::string::npos);
	EXPECT_EQ(output.find("warp::codegen::deduced_body_response_contract<"), std::string::npos);
	EXPECT_EQ(output.find("static_cast<const generated_api::users_create_user_response_body "
	                      "&(generated_api::users_create_user_response::*)() const & noexcept>"
	                      "(&generated_api::users_create_user_response::body)"),
	          std::string::npos);
	EXPECT_NE(output.find("struct response_contract_traits<generated_api::users_health_response> : "
	                      "warp::codegen::empty_response_contract<generated_api::users_health_response> {};"),
	          std::string::npos);
	EXPECT_NE(output.find("using users_create_user_request_endpoint = warp::codegen::generated_endpoint_binding<"),
	          std::string::npos);
	EXPECT_NE(output.find("using users_create_user_request_handler_result = "
	                      "warp::codegen::handler_result<generated_api::users_create_user_response>;"),
	          std::string::npos);
	EXPECT_NE(output.find("using users_api_routes = warp::codegen::generated_resource<"), std::string::npos);
	EXPECT_NE(output.find("struct users_create_user_request_handler_selector {"), std::string::npos);
	EXPECT_NE(output.find("warp::codegen::request_contract_traits<generated_api::users_create_user_request>,"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::response_contract_traits<generated_api::users_create_user_response>,"),
	          std::string::npos);
	EXPECT_NE(output.find("generated_api::codegen_detail::users_create_user_request_handler_selector>;"),
	          std::string::npos);
	EXPECT_EQ(output.find("struct users_create_user_request_user_id_accessor {"), std::string::npos);
	EXPECT_EQ(output.find("struct users_create_user_response_body_accessor {"), std::string::npos);
	EXPECT_EQ(output.find("using users_create_user_request_contract ="), std::string::npos);
	EXPECT_EQ(output.find("using users_create_user_response_contract ="), std::string::npos);
	EXPECT_EQ(output.find("[](Service &service, generated_api::users_create_user_request &&typed_request) "
	                      "-> decltype(auto) {"),
	          std::string::npos);
}

TEST(ResourceEmitterTest, EmitsTypedDispatchBindingsForOverloadedHandlerNames) {
	const auto spec = parse_api_spec(R"(
cpp_namespace: generated_api
resources:
  - name: users
    endpoints:
      - name: health
        method: GET
        path: /users/{user_id}/health
        request:
          parameters:
            - name: user_id
              in: path
              type: string
        response:
          status: 200
  - name: admin
    endpoints:
      - name: health
        method: GET
        path: /admin/health
        response:
          status: 204
)");

	const auto output = resource_emitter().emit_header(build_api_model(spec));

	EXPECT_NE(output.find("struct users_health_request_handler_selector {"), std::string::npos);
	EXPECT_NE(output.find("struct admin_health_request_handler_selector {"), std::string::npos);
	EXPECT_NE(output.find("using users_health_request_endpoint = warp::codegen::generated_endpoint_binding<"),
	          std::string::npos);
	EXPECT_NE(output.find("using admin_health_request_endpoint = warp::codegen::generated_endpoint_binding<"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::request_contract_traits<generated_api::users_health_request>,"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::response_contract_traits<generated_api::admin_health_response>,"),
	          std::string::npos);
	EXPECT_NE(output.find("generated_api::codegen_detail::users_health_request_handler_selector>;"), std::string::npos);
	EXPECT_NE(output.find("generated_api::codegen_detail::admin_health_request_handler_selector>;"), std::string::npos);
	EXPECT_NE(output.find("static_cast<Signature>(&Service::health);"), std::string::npos);
	EXPECT_EQ(output.find("[](Service &service, generated_api::users_health_request &&typed_request) "
	                      "-> decltype(auto) {"),
	          std::string::npos);
	EXPECT_EQ(output.find("[](Service &service, generated_api::admin_health_request &&typed_request) "
	                      "-> decltype(auto) {"),
	          std::string::npos);
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

	EXPECT_NE(
	    output.find(
	        R"(using users_fetch_user_request_route = warp::http::route_spec<warp::method::get, "/users/\"quoted\"">;)"),
	    std::string::npos);
	EXPECT_NE(
	    output.find(
	        R"(warp::codegen::header_setter_binding<&generated_api::users_fetch_user_request::set_x_trace, "x-\"trace\"">)"),
	    std::string::npos);
}

TEST(ResourceEmitterTest, EmitsNamespaceQualifiedTraitSpecializationsWithoutAliasGlue) {
	const auto primary_spec = parse_api_spec(R"(
cpp_namespace: generated_primary_api
resources:
  - name: users
    endpoints:
      - name: health
        method: GET
        path: /health
        response:
          status: 204
)");
	const auto secondary_spec = parse_api_spec(R"(
cpp_namespace: generated_secondary_api
resources:
  - name: users
    endpoints:
      - name: health
        method: GET
        path: /health
        response:
          status: 204
)");

	const auto primary_output = resource_emitter().emit_header(build_api_model(primary_spec));
	const auto secondary_output = resource_emitter().emit_header(build_api_model(secondary_spec));

	EXPECT_NE(primary_output.find("namespace generated_primary_api::codegen_detail {"), std::string::npos);
	EXPECT_NE(primary_output.find("struct request_contract_traits<generated_primary_api::users_health_request> : "
	                              "warp::codegen::generated_request_contract<"),
	          std::string::npos);
	EXPECT_NE(secondary_output.find("namespace generated_secondary_api::codegen_detail {"), std::string::npos);
	EXPECT_NE(secondary_output.find("struct request_contract_traits<generated_secondary_api::users_health_request> : "
	                                "warp::codegen::generated_request_contract<"),
	          std::string::npos);
	EXPECT_EQ(primary_output.find("struct request_contract_traits<generated_primary_api::users_health_request> : "
	                              "users_health_request_contract {};"),
	          std::string::npos);
	EXPECT_EQ(secondary_output.find("struct request_contract_traits<generated_secondary_api::users_health_request> : "
	                                "users_health_request_contract {};"),
	          std::string::npos);
	EXPECT_EQ(primary_output.find("struct request_contract_traits<generated_primary_api::users_health_request> : "
	                              "generated_primary_api::users_health_request_contract {};"),
	          std::string::npos);
	EXPECT_EQ(secondary_output.find("struct request_contract_traits<generated_secondary_api::users_health_request> : "
	                                "generated_secondary_api::users_health_request_contract {};"),
	          std::string::npos);
	EXPECT_EQ(primary_output.find("using users_health_request_contract ="), std::string::npos);
	EXPECT_EQ(secondary_output.find("using users_health_request_contract ="), std::string::npos);
}

TEST(ResourceEmitterTest, EmitsQueryRouteSpecsForOverlappingGeneratedRoutes) {
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
            - name: summary
              in: query
              type: bool
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
            - name: fields
              in: query
              type: string
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
            - name: summary
              in: query
              type: bool
            - name: fields
              in: query
              type: string
        response:
          status: 204
)");

	const auto output = resource_emitter().emit_header(build_api_model(spec, "generated_api"));

	EXPECT_NE(output.find("using reports_fetch_report_summary_query_route = warp::http::route_spec<"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::http::required_query<\"summary\">"), std::string::npos);
	EXPECT_NE(output.find("warp::http::required_query<\"fields\">"), std::string::npos);
	EXPECT_NE(output.find("static_assert(warp::http::deterministic_route_definitions<"), std::string::npos);
	EXPECT_NE(output.find("reports_fetch_report_summary_query_route,"), std::string::npos);
	EXPECT_NE(output.find("reports_fetch_report_summary_projection_query_route,"), std::string::npos);
	EXPECT_NE(output.find("using reports_api_routes = warp::codegen::generated_resource<"), std::string::npos);
}

TEST(ResourceEmitterTest, KeepsSingletonRequiredQueryRoutesUnconstrained) {
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
            - name: summary
              in: query
              type: bool
              required: true
        response:
          status: 204
)");

	const auto output = resource_emitter().emit_header(build_api_model(spec, "generated_api"));

	EXPECT_NE(output.find("using reports_fetch_report_summary_request_route = "
	                      "warp::http::route_spec<warp::method::get, \"/reports/{report_id}\">;"),
	          std::string::npos);
	EXPECT_EQ(output.find("reports_fetch_report_summary_query_route"), std::string::npos);
	EXPECT_EQ(output.find("warp::http::required_query<\"summary\">"), std::string::npos);
}

} // namespace
