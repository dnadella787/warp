#pragma once

#include "generated_api_types.hpp"
#include "warp/codegen/http_adapter.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace warp::codegen {

template <>
struct request_contract_traits<generated_api::users_create_user_request> {
	static parse_result<generated_api::users_create_user_request> parse(const request &req) {
		generated_api::users_create_user_request out;
		auto parsed_user_id = required_path_param<std::string>(req, "user_id");
		if (!parsed_user_id.has_value()) {
			return parse_result<generated_api::users_create_user_request>::failure(parsed_user_id.error());
		}
		out.user_id = std::move(parsed_user_id).value();
		auto parsed_verbose = optional_query_param<bool>(req, "verbose");
		if (!parsed_verbose.has_value()) {
			return parse_result<generated_api::users_create_user_request>::failure(parsed_verbose.error());
		}
		out.verbose = std::move(parsed_verbose).value();
		auto parsed_x_trace_id = required_header_param<std::string>(req, "x-trace-id");
		if (!parsed_x_trace_id.has_value()) {
			return parse_result<generated_api::users_create_user_request>::failure(parsed_x_trace_id.error());
		}
		out.x_trace_id = std::move(parsed_x_trace_id).value();
		auto parsed_body = json_body<generated_api::users_create_user_request_body>(req);
		if (!parsed_body.has_value()) {
			return parse_result<generated_api::users_create_user_request>::failure(parsed_body.error());
		}
		out.body = std::move(parsed_body).value();
		return parse_result<generated_api::users_create_user_request>::success(std::move(out));
	}
};

template <>
struct response_contract_traits<generated_api::users_create_user_response> {
	static constexpr unsigned status_code = generated_api::users_create_user_response::status_code;
	static constexpr bool has_body = true;
	static const generated_api::users_create_user_response_body &
	body(const generated_api::users_create_user_response &value) {
		return value.body;
	}
};

template <>
struct request_contract_traits<generated_api::users_health_request> {
	static parse_result<generated_api::users_health_request> parse(const request &req) {
		generated_api::users_health_request out;
		return parse_result<generated_api::users_health_request>::success(std::move(out));
	}
};

template <>
struct response_contract_traits<generated_api::users_health_response> {
	static constexpr unsigned status_code = generated_api::users_health_response::status_code;
	static constexpr bool has_body = false;
};

} // namespace warp::codegen

namespace generated_api {

template <typename Service>
class users_api_routes {
public:
	explicit users_api_routes(std::shared_ptr<Service> service) : service_(std::move(service)) {
		if (!service_) {
			throw std::invalid_argument("service must not be null");
		}
	}

	void register_routes(warp::http::server_builder &builder) const {
		builder.route(warp::method::post, "/users/{user_id}",
		              [service = service_](warp::request req) -> warp::awaitable<warp::response> {
			              const auto version = req.version();
			              const auto keep_alive = req.keep_alive();
			              auto typed_request =
			                  warp::codegen::parse_http_request<generated_api::users_create_user_request>(req);
			              if (!typed_request.has_value()) {
				              auto response = warp::codegen::to_error_response(typed_request.error(), version);
				              response.keep_alive(keep_alive);
				              co_return response;
			              }
			              auto typed_response =
			                  co_await warp::codegen::invoke_user_handler<generated_api::users_create_user_response>(
			                      [service, typed_request = std::move(typed_request).value()]() mutable {
				                      return service->create_user(std::move(typed_request));
			                      });
			              auto response = warp::codegen::to_http_response(typed_response, version);
			              response.keep_alive(keep_alive);
			              co_return response;
		              });
		builder.route(
		    warp::method::get, "/health", [service = service_](warp::request req) -> warp::awaitable<warp::response> {
			    const auto version = req.version();
			    const auto keep_alive = req.keep_alive();
			    auto typed_request = warp::codegen::parse_http_request<generated_api::users_health_request>(req);
			    if (!typed_request.has_value()) {
				    auto response = warp::codegen::to_error_response(typed_request.error(), version);
				    response.keep_alive(keep_alive);
				    co_return response;
			    }
			    auto typed_response = co_await warp::codegen::invoke_user_handler<generated_api::users_health_response>(
			        [service, typed_request = std::move(typed_request).value()]() mutable {
				        return service->health(std::move(typed_request));
			        });
			    auto response = warp::codegen::to_http_response(typed_response, version);
			    response.keep_alive(keep_alive);
			    co_return response;
		    });
	}

private:
	std::shared_ptr<Service> service_;
};

} // namespace generated_api
