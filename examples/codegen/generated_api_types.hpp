#pragma once

#include "warp/codegen/json_object_contract.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace generated_api {

struct users_create_user_request_body {
	std::string name {};
	std::optional<std::string> nickname {};
};

struct users_create_user_response_body {
	std::int64_t id {};
	bool active {};
};

struct users_create_user_request {
	std::string user_id {};
	std::optional<bool> verbose {};
	std::optional<std::string> filter {};
	std::string x_trace_id {};
	users_create_user_request_body body {};
};

struct users_create_user_response {
	users_create_user_response_body body {};
	static constexpr unsigned status_code = 201;
};

struct users_health_request {};

struct users_health_response {
	static constexpr unsigned status_code = 204;
};

template <typename T>
    requires warp::codegen::json_contract_type<T>
inline T tag_invoke(boost::json::value_to_tag<T>, const boost::json::value &value) {
	return warp::codegen::parse_json_object<T>(value);
}

template <typename T>
    requires warp::codegen::json_contract_type<T>
inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value, T &&input) {
	warp::codegen::serialize_json_object(value, std::forward<T>(input));
}

} // namespace generated_api

namespace warp::codegen {

template <>
struct json_object_contract<generated_api::users_create_user_request_body> {
	static constexpr std::string_view type_name = "users_create_user_request_body";
	static constexpr auto fields =
	    std::make_tuple(make_required_json_field("name", &generated_api::users_create_user_request_body::name,
	                                             json_field_validation<std::string> {.min_length = 3}),
	                    make_optional_json_field("nickname", &generated_api::users_create_user_request_body::nickname,
	                                             json_field_validation<std::string> {.max_length = 5}));
};

template <>
struct json_object_contract<generated_api::users_create_user_response_body> {
	static constexpr std::string_view type_name = "users_create_user_response_body";
	static constexpr auto fields =
	    std::make_tuple(make_required_json_field("id", &generated_api::users_create_user_response_body::id),
	                    make_required_json_field("active", &generated_api::users_create_user_response_body::active));
};

} // namespace warp::codegen
