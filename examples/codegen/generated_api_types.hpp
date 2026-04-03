#pragma once

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace generated_api {

struct users_create_user_request_body {
	std::string name {};
	std::optional<std::string> nickname {};
};

inline users_create_user_request_body tag_invoke(boost::json::value_to_tag<users_create_user_request_body>,
                                                 const boost::json::value &value) {
	const auto &obj = value.as_object();
	users_create_user_request_body out;
	const auto *raw_name = obj.if_contains("name");
	if (raw_name == nullptr) {
		throw std::invalid_argument("missing required field 'name' for users_create_user_request_body");
	}
	out.name = boost::json::value_to<std::string>(*raw_name);
	const auto *raw_nickname = obj.if_contains("nickname");
	if (raw_nickname != nullptr) {
		out.nickname = boost::json::value_to<std::string>(*raw_nickname);
	}
	return out;
}

inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value,
                       const users_create_user_request_body &input) {
	boost::json::object obj;
	obj["name"] = boost::json::value_from(input.name);
	if (input.nickname.has_value()) {
		obj["nickname"] = boost::json::value_from(*input.nickname);
	}
	value = std::move(obj);
}

struct users_create_user_response_body {
	std::int64_t id {};
	bool active {};
};

inline users_create_user_response_body tag_invoke(boost::json::value_to_tag<users_create_user_response_body>,
                                                  const boost::json::value &value) {
	const auto &obj = value.as_object();
	users_create_user_response_body out;
	const auto *raw_id = obj.if_contains("id");
	if (raw_id == nullptr) {
		throw std::invalid_argument("missing required field 'id' for users_create_user_response_body");
	}
	out.id = boost::json::value_to<std::int64_t>(*raw_id);
	const auto *raw_active = obj.if_contains("active");
	if (raw_active == nullptr) {
		throw std::invalid_argument("missing required field 'active' for users_create_user_response_body");
	}
	out.active = boost::json::value_to<bool>(*raw_active);
	return out;
}

inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value,
                       const users_create_user_response_body &input) {
	boost::json::object obj;
	obj["id"] = boost::json::value_from(input.id);
	obj["active"] = boost::json::value_from(input.active);
	value = std::move(obj);
}

struct users_create_user_request {
	std::string user_id {};
	std::optional<bool> verbose {};
	std::string x_trace_id {};
	users_create_user_request_body body {};
};

struct users_create_user_response {
	static constexpr unsigned status_code = 201;
	users_create_user_response_body body {};
};

struct users_health_request {};

struct users_health_response {
	static constexpr unsigned status_code = 204;
};

} // namespace generated_api
