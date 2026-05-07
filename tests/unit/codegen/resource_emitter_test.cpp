#include "codegen/resource_emitter.hpp"
#include "codegen/model.hpp"
#include "codegen/spec_parser.hpp"
#include "warp/codegen/http_adapter.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

namespace {

using warp::codegen::build_api_model;
using warp::codegen::parse_api_spec;
using warp::codegen::resource_emitter;

struct validation_test_request {
	std::optional<std::int64_t> limit;
	std::string trace_id;
};

struct validation_test_request_validator {
	using request_type = validation_test_request;

	static std::optional<warp::codegen::binding_error> validate(const request_type &value) {
		if (value.limit.has_value()) {
			if (auto error = warp::codegen::validate_min_value("query parameter", "limit", *value.limit, 1);
			    error.has_value()) {
				return error;
			}
		}

		if (auto error = warp::codegen::validate_min_length("header", "x-trace-id", value.trace_id, 3);
		    error.has_value()) {
			return error;
		}

		return std::nullopt;
	}
};

using validation_test_base_contract = warp::codegen::generated_request_contract<
    validation_test_request, warp::codegen::query_field_binding<&validation_test_request::limit, "limit">,
    warp::codegen::header_field_binding<&validation_test_request::trace_id, "x-trace-id">>;

using validation_test_contract =
    warp::codegen::validated_request_contract<validation_test_base_contract, validation_test_request_validator>;

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
	EXPECT_NE(output.find("warp::codegen::generated_request_contract<"), std::string::npos);
	EXPECT_EQ(output.find("struct users_create_user_request_validator {"), std::string::npos);
	EXPECT_EQ(output.find("struct users_create_user_request_body_validator {"), std::string::npos);
	EXPECT_EQ(output.find("if (value.verbose.has_value()) {\n        }"), std::string::npos);
	EXPECT_NE(output.find("warp::codegen::path_field_binding<&generated_api::users_create_user_request::user_id, "
	                      "\"user_id\">"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::query_field_binding<&generated_api::users_create_user_request::verbose, "
	                      "\"verbose\">"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::header_field_binding<&generated_api::users_create_user_request::x_trace_id, "
	                      "\"x-trace-id\">"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::codegen::json_body_field_binding<&generated_api::users_create_user_request::body>"),
	          std::string::npos);
	EXPECT_NE(output.find("struct response_contract_traits<generated_api::users_create_user_response> {"),
	          std::string::npos);
	EXPECT_NE(output.find("using response_type = generated_api::users_create_user_response;"), std::string::npos);
	EXPECT_NE(output.find("static constexpr unsigned status_code = response_type::status_code;"), std::string::npos);
	EXPECT_NE(output.find("static constexpr bool has_body = true;"), std::string::npos);
	EXPECT_NE(output.find("static decltype(auto) body(const response_type &value) {"), std::string::npos);
	EXPECT_NE(output.find("return (value.body);"), std::string::npos);
	EXPECT_NE(output.find("static decltype(auto) body(response_type &&value) {"), std::string::npos);
	EXPECT_NE(output.find("return (std::move(value).body);"), std::string::npos);
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
	        R"(warp::codegen::header_field_binding<&generated_api::users_fetch_user_request::x_trace, "x-\"trace\"">)"),
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
	EXPECT_NE(secondary_output.find("namespace generated_secondary_api::codegen_detail {"), std::string::npos);
	EXPECT_EQ(primary_output.find("struct request_contract_traits<generated_primary_api::users_health_request> : "
	                              "warp::codegen::validated_request_contract<"),
	          std::string::npos);
	EXPECT_EQ(secondary_output.find("struct request_contract_traits<generated_secondary_api::users_health_request> : "
	                                "warp::codegen::validated_request_contract<"),
	          std::string::npos);
	EXPECT_NE(primary_output.find("struct request_contract_traits<generated_primary_api::users_health_request> : "
	                              "warp::codegen::generated_request_contract<"),
	          std::string::npos);
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
	EXPECT_EQ(primary_output.find("struct users_health_request_validator {"), std::string::npos);
	EXPECT_EQ(secondary_output.find("struct users_health_request_validator {"), std::string::npos);
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
	EXPECT_NE(output.find("using reports_fetch_report_summary_query_route = warp::http::route_spec<warp::method::get, "
	                      "\"/reports/{report_id}\", warp::http::required_query<\"summary\">, "
	                      "warp::http::forbidden_query<\"fields\">>;"),
	          std::string::npos);
	EXPECT_NE(
	    output.find("using reports_fetch_report_projection_query_route = warp::http::route_spec<warp::method::get, "
	                "\"/reports/{report_id}\", warp::http::forbidden_query<\"summary\">, "
	                "warp::http::required_query<\"fields\">>;"),
	    std::string::npos);
	EXPECT_NE(output.find("using reports_fetch_report_summary_projection_query_route = warp::http::route_spec<"
	                      "warp::method::get, \"/reports/{report_id}\", warp::http::required_query<\"summary\">, "
	                      "warp::http::required_query<\"fields\">>;"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::http::required_query<\"summary\">"), std::string::npos);
	EXPECT_NE(output.find("warp::http::required_query<\"fields\">"), std::string::npos);
	EXPECT_NE(output.find("warp::http::forbidden_query<\"fields\">"), std::string::npos);
	EXPECT_NE(output.find("warp::http::forbidden_query<\"summary\">"), std::string::npos);
	EXPECT_NE(output.find("static_assert(warp::http::deterministic_route_definitions<"), std::string::npos);
	EXPECT_NE(output.find("reports_fetch_report_summary_query_route,"), std::string::npos);
	EXPECT_NE(output.find("reports_fetch_report_summary_projection_query_route,"), std::string::npos);
	EXPECT_NE(output.find("using reports_api_routes = warp::codegen::generated_resource<"), std::string::npos);
}

TEST(ResourceEmitterTest, EmitsSingletonRequiredQueryRoutesWithConstraints) {
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

	EXPECT_NE(output.find("using reports_fetch_report_summary_query_route = "
	                      "warp::http::route_spec<warp::method::get, \"/reports/{report_id}\", "
	                      "warp::http::required_query<\"summary\">>;"),
	          std::string::npos);
	EXPECT_NE(output.find("using reports_fetch_report_summary_query_route = warp::http::route_spec<"),
	          std::string::npos);
	EXPECT_NE(output.find("warp::http::required_query<\"summary\">"), std::string::npos);
	EXPECT_EQ(output.find("using reports_fetch_report_summary_request_route"), std::string::npos);
}

TEST(ResourceEmitterTest, ValidatedRequestContractRunsPostBindingChecksWithWireNames) {
	warp::request valid_request(boost::beast::http::verb::get, "/reports", 11);
	valid_request.set("x-trace-id", "abc");

	auto valid_result = validation_test_contract::parse(valid_request);
	ASSERT_TRUE(valid_result.has_value());
	EXPECT_FALSE(valid_result.value().limit.has_value());
	EXPECT_EQ(valid_result.value().trace_id, "abc");

	warp::request invalid_query_request(boost::beast::http::verb::get, "/reports?limit=0", 11);
	invalid_query_request.set("x-trace-id", "abcdef");

	auto invalid_query_result = validation_test_contract::parse(invalid_query_request);
	ASSERT_FALSE(invalid_query_result.has_value());
	EXPECT_EQ(invalid_query_result.error().code, "constraint_violation");
	EXPECT_EQ(invalid_query_result.error().message, "invalid query parameter 'limit': must be >= 1");

	warp::request invalid_header_request(boost::beast::http::verb::get, "/reports", 11);
	invalid_header_request.set("x-trace-id", "ab");

	auto invalid_header_result = validation_test_contract::parse(invalid_header_request);
	ASSERT_FALSE(invalid_header_result.has_value());
	EXPECT_EQ(invalid_header_result.error().code, "constraint_violation");
	EXPECT_EQ(invalid_header_result.error().message, "invalid header 'x-trace-id': length must be >= 3");
}

} // namespace
