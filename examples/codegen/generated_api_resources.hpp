#pragma once

#include "generated_api_types.hpp"
#include "warp/codegen/http_adapter.hpp"

namespace generated_api::codegen_detail {

struct users_create_user_request_handler_selector {
	template <typename Signature, typename Service>
	static consteval bool matches() {
		return requires { static_cast<Signature>(&Service::create_user); };
	}
	template <typename Signature, typename Service>
	static constexpr Signature get() {
		return static_cast<Signature>(&Service::create_user);
	}
};

struct users_health_request_handler_selector {
	template <typename Signature, typename Service>
	static consteval bool matches() {
		return requires { static_cast<Signature>(&Service::health); };
	}
	template <typename Signature, typename Service>
	static constexpr Signature get() {
		return static_cast<Signature>(&Service::health);
	}
};

} // namespace generated_api::codegen_detail

namespace warp::codegen {

template <>
struct request_contract_traits<generated_api::users_create_user_request>
    : warp::codegen::generated_request_contract<
          generated_api::users_create_user_request,
          warp::codegen::path_setter_binding<&generated_api::users_create_user_request::set_user_id, "user_id">,
          warp::codegen::query_setter_binding<&generated_api::users_create_user_request::set_verbose, "verbose">,
          warp::codegen::header_setter_binding<&generated_api::users_create_user_request::set_x_trace_id, "x-trace-id">,
          warp::codegen::json_body_setter_binding<&generated_api::users_create_user_request::set_body>> {};

template <>
struct response_contract_traits<generated_api::users_create_user_response> {
	using response_type = generated_api::users_create_user_response;
	static constexpr unsigned status_code = response_type::status_code;
	static constexpr bool has_body = true;

	static decltype(auto) body(const response_type &value) {
		return value.body();
	}

	static decltype(auto) body(response_type &&value) {
		return std::move(value).body();
	}
};

template <>
struct request_contract_traits<generated_api::users_health_request>
    : warp::codegen::generated_request_contract<generated_api::users_health_request> {};

template <>
struct response_contract_traits<generated_api::users_health_response>
    : warp::codegen::empty_response_contract<generated_api::users_health_response> {};

} // namespace warp::codegen

namespace generated_api {

using users_create_user_request_route = warp::http::route_spec<warp::method::post, "/users/{user_id}">;
using users_health_request_route = warp::http::route_spec<warp::method::get, "/health">;

using users_create_user_request_handler_result =
    warp::codegen::handler_result<generated_api::users_create_user_response>;
using users_health_request_handler_result = warp::codegen::handler_result<generated_api::users_health_response>;

template <typename Service>
using users_create_user_request_endpoint = warp::codegen::generated_endpoint_binding<
    Service, users_create_user_request_route,
    warp::codegen::request_contract_traits<generated_api::users_create_user_request>,
    warp::codegen::response_contract_traits<generated_api::users_create_user_response>,
    generated_api::codegen_detail::users_create_user_request_handler_selector>;

template <typename Service>
using users_health_request_endpoint = warp::codegen::generated_endpoint_binding<
    Service, users_health_request_route, warp::codegen::request_contract_traits<generated_api::users_health_request>,
    warp::codegen::response_contract_traits<generated_api::users_health_response>,
    generated_api::codegen_detail::users_health_request_handler_selector>;

template <typename Service>
using users_api_routes = warp::codegen::generated_resource<Service, users_create_user_request_endpoint<Service>,
                                                           users_health_request_endpoint<Service>>;

} // namespace generated_api
