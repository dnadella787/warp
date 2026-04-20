#pragma once

#include "generated_api_types.hpp"
#include "warp/codegen/http_adapter.hpp"

namespace generated_api::codegen_detail {

struct users_create_user_request_user_id_accessor {
	using class_type = users_create_user_request;
	using value_type = std::string;
	static void set(class_type &value, value_type member_value) {
		value.set_user_id(std::move(member_value));
	}
};

struct users_create_user_request_verbose_accessor {
	using class_type = users_create_user_request;
	using value_type = std::optional<bool>;
	static void set(class_type &value, value_type member_value) {
		value.set_verbose(std::move(member_value));
	}
};

struct users_create_user_request_x_trace_id_accessor {
	using class_type = users_create_user_request;
	using value_type = std::string;
	static void set(class_type &value, value_type member_value) {
		value.set_x_trace_id(std::move(member_value));
	}
};

struct users_create_user_request_body_accessor {
	using class_type = users_create_user_request;
	using value_type = users_create_user_request_body;
	static void set(class_type &value, value_type member_value) {
		value.set_body(std::move(member_value));
	}
};

struct users_create_user_response_body_accessor {
	using class_type = users_create_user_response;
	using value_type = users_create_user_response_body;
	[[nodiscard]] static const value_type &get(const class_type &value) noexcept {
		return value.body();
	}
	[[nodiscard]] static value_type &&get(class_type &&value) noexcept {
		return std::move(value).body();
	}
};

using users_create_user_request_contract = warp::codegen::generated_request_contract<
    users_create_user_request, warp::codegen::path_binding<users_create_user_request_user_id_accessor, "user_id">,
    warp::codegen::query_binding<users_create_user_request_verbose_accessor, "verbose">,
    warp::codegen::header_binding<users_create_user_request_x_trace_id_accessor, "x-trace-id">,
    warp::codegen::json_body_binding<users_create_user_request_body_accessor>>;
using users_create_user_response_contract =
    warp::codegen::body_response_contract<users_create_user_response, users_create_user_response_body_accessor>;
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

using users_health_request_contract = warp::codegen::generated_request_contract<users_health_request>;
using users_health_response_contract = warp::codegen::empty_response_contract<users_health_response>;
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
    : generated_api::codegen_detail::users_create_user_request_contract {};

template <>
struct response_contract_traits<generated_api::users_create_user_response>
    : generated_api::codegen_detail::users_create_user_response_contract {};

template <>
struct request_contract_traits<generated_api::users_health_request>
    : generated_api::codegen_detail::users_health_request_contract {};

template <>
struct response_contract_traits<generated_api::users_health_response>
    : generated_api::codegen_detail::users_health_response_contract {};

} // namespace warp::codegen

namespace generated_api {

using users_create_user_request_route = warp::http::route_spec<warp::method::post, "/users/{user_id}">;
using users_health_request_route = warp::http::route_spec<warp::method::get, "/health">;

template <typename Service>
using users_create_user_request_endpoint = warp::codegen::endpoint_binding<
    Service, users_create_user_request_route,
    warp::codegen::request_contract_traits<generated_api::users_create_user_request>,
    generated_api::users_create_user_response,
    [](Service &service, generated_api::users_create_user_request &&typed_request) -> decltype(auto) {
	    return warp::codegen::invoke_endpoint_handler_overload<
	        generated_api::users_create_user_response, generated_api::users_create_user_request, Service,
	        generated_api::codegen_detail::users_create_user_request_handler_selector>(service,
	                                                                                   std::move(typed_request));
    }>;

template <typename Service>
using users_health_request_endpoint = warp::codegen::endpoint_binding<
    Service, users_health_request_route, warp::codegen::request_contract_traits<generated_api::users_health_request>,
    generated_api::users_health_response,
    [](Service &service, generated_api::users_health_request &&typed_request) -> decltype(auto) {
	    return warp::codegen::invoke_endpoint_handler_overload<
	        generated_api::users_health_response, generated_api::users_health_request, Service,
	        generated_api::codegen_detail::users_health_request_handler_selector>(service, std::move(typed_request));
    }>;

template <typename Service>
using users_api_routes = warp::codegen::generated_resource<Service, users_create_user_request_endpoint<Service>,
                                                           users_health_request_endpoint<Service>>;

} // namespace generated_api
