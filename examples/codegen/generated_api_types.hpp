#pragma once

#include "warp/codegen/json_object_contract.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace generated_api {

class users_create_user_request_body {
public:
	class Builder {
	public:
		Builder &name(std::string value) {
			name_ = std::move(value);
			return *this;
		}
		Builder &nickname(std::optional<std::string> value) {
			nickname_ = std::move(value);
			return *this;
		}

		[[nodiscard]] users_create_user_request_body build() && {
			users_create_user_request_body out;
			out.name_ = std::move(name_);
			out.nickname_ = std::move(nickname_);
			return out;
		}

		[[nodiscard]] users_create_user_request_body build() const & {
			users_create_user_request_body out;
			out.name_ = name_;
			out.nickname_ = nickname_;
			return out;
		}

	private:
		std::string name_ {};
		std::optional<std::string> nickname_ {};
	};

	users_create_user_request_body() = default;
	[[nodiscard]] static Builder builder() {
		return Builder {};
	}

	[[nodiscard]] const std::string &name() const & noexcept {
		return name_;
	}

	[[nodiscard]] std::string &name() & noexcept {
		return name_;
	}

	[[nodiscard]] std::string &&name() && noexcept {
		return std::move(name_);
	}

	users_create_user_request_body &set_name(std::string value) {
		name_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const std::optional<std::string> &nickname() const & noexcept {
		return nickname_;
	}

	[[nodiscard]] std::optional<std::string> &nickname() & noexcept {
		return nickname_;
	}

	[[nodiscard]] std::optional<std::string> &&nickname() && noexcept {
		return std::move(nickname_);
	}

	users_create_user_request_body &set_nickname(std::optional<std::string> value) {
		nickname_ = std::move(value);
		return *this;
	}

private:
	std::string name_ {};
	std::optional<std::string> nickname_ {};
};

class users_create_user_response_body {
public:
	class Builder {
	public:
		Builder &id(std::int64_t value) {
			id_ = std::move(value);
			return *this;
		}
		Builder &active(bool value) {
			active_ = std::move(value);
			return *this;
		}

		[[nodiscard]] users_create_user_response_body build() && {
			users_create_user_response_body out;
			out.id_ = std::move(id_);
			out.active_ = std::move(active_);
			return out;
		}

		[[nodiscard]] users_create_user_response_body build() const & {
			users_create_user_response_body out;
			out.id_ = id_;
			out.active_ = active_;
			return out;
		}

	private:
		std::int64_t id_ {};
		bool active_ {};
	};

	users_create_user_response_body() = default;
	[[nodiscard]] static Builder builder() {
		return Builder {};
	}

	[[nodiscard]] const std::int64_t &id() const & noexcept {
		return id_;
	}

	[[nodiscard]] std::int64_t &id() & noexcept {
		return id_;
	}

	[[nodiscard]] std::int64_t &&id() && noexcept {
		return std::move(id_);
	}

	users_create_user_response_body &set_id(std::int64_t value) {
		id_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const bool &active() const & noexcept {
		return active_;
	}

	[[nodiscard]] bool &active() & noexcept {
		return active_;
	}

	[[nodiscard]] bool &&active() && noexcept {
		return std::move(active_);
	}

	users_create_user_response_body &set_active(bool value) {
		active_ = std::move(value);
		return *this;
	}

private:
	std::int64_t id_ {};
	bool active_ {};
};

class users_create_user_request {
public:
	class Builder {
	public:
		Builder &user_id(std::string value) {
			user_id_ = std::move(value);
			return *this;
		}
		Builder &verbose(std::optional<bool> value) {
			verbose_ = std::move(value);
			return *this;
		}
		Builder &x_trace_id(std::string value) {
			x_trace_id_ = std::move(value);
			return *this;
		}
		Builder &body(users_create_user_request_body value) {
			body_ = std::move(value);
			return *this;
		}

		[[nodiscard]] users_create_user_request build() && {
			users_create_user_request out;
			out.user_id_ = std::move(user_id_);
			out.verbose_ = std::move(verbose_);
			out.x_trace_id_ = std::move(x_trace_id_);
			out.body_ = std::move(body_);
			return out;
		}

		[[nodiscard]] users_create_user_request build() const & {
			users_create_user_request out;
			out.user_id_ = user_id_;
			out.verbose_ = verbose_;
			out.x_trace_id_ = x_trace_id_;
			out.body_ = body_;
			return out;
		}

	private:
		std::string user_id_ {};
		std::optional<bool> verbose_ {};
		std::string x_trace_id_ {};
		users_create_user_request_body body_ {};
	};

	users_create_user_request() = default;
	[[nodiscard]] static Builder builder() {
		return Builder {};
	}

	[[nodiscard]] const std::string &user_id() const & noexcept {
		return user_id_;
	}

	[[nodiscard]] std::string &user_id() & noexcept {
		return user_id_;
	}

	[[nodiscard]] std::string &&user_id() && noexcept {
		return std::move(user_id_);
	}

	users_create_user_request &set_user_id(std::string value) {
		user_id_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const std::optional<bool> &verbose() const & noexcept {
		return verbose_;
	}

	[[nodiscard]] std::optional<bool> &verbose() & noexcept {
		return verbose_;
	}

	[[nodiscard]] std::optional<bool> &&verbose() && noexcept {
		return std::move(verbose_);
	}

	users_create_user_request &set_verbose(std::optional<bool> value) {
		verbose_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const std::string &x_trace_id() const & noexcept {
		return x_trace_id_;
	}

	[[nodiscard]] std::string &x_trace_id() & noexcept {
		return x_trace_id_;
	}

	[[nodiscard]] std::string &&x_trace_id() && noexcept {
		return std::move(x_trace_id_);
	}

	users_create_user_request &set_x_trace_id(std::string value) {
		x_trace_id_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const users_create_user_request_body &body() const & noexcept {
		return body_;
	}

	[[nodiscard]] users_create_user_request_body &body() & noexcept {
		return body_;
	}

	[[nodiscard]] users_create_user_request_body &&body() && noexcept {
		return std::move(body_);
	}

	users_create_user_request &set_body(users_create_user_request_body value) {
		body_ = std::move(value);
		return *this;
	}

private:
	std::string user_id_ {};
	std::optional<bool> verbose_ {};
	std::string x_trace_id_ {};
	users_create_user_request_body body_ {};
};

class users_create_user_response {
public:
	class Builder {
	public:
		Builder &body(users_create_user_response_body value) {
			body_ = std::move(value);
			return *this;
		}

		[[nodiscard]] users_create_user_response build() && {
			users_create_user_response out;
			out.body_ = std::move(body_);
			return out;
		}

		[[nodiscard]] users_create_user_response build() const & {
			users_create_user_response out;
			out.body_ = body_;
			return out;
		}

	private:
		users_create_user_response_body body_ {};
	};

	users_create_user_response() = default;
	static constexpr unsigned status_code = 201;
	[[nodiscard]] static Builder builder() {
		return Builder {};
	}

	[[nodiscard]] const users_create_user_response_body &body() const & noexcept {
		return body_;
	}

	[[nodiscard]] users_create_user_response_body &body() & noexcept {
		return body_;
	}

	[[nodiscard]] users_create_user_response_body &&body() && noexcept {
		return std::move(body_);
	}

	users_create_user_response &set_body(users_create_user_response_body value) {
		body_ = std::move(value);
		return *this;
	}

private:
	users_create_user_response_body body_ {};
};

class users_health_request {
public:
	class Builder {
	public:
		[[nodiscard]] users_health_request build() && {
			users_health_request out;
			return out;
		}

		[[nodiscard]] users_health_request build() const & {
			users_health_request out;
			return out;
		}

	private:
	};

	users_health_request() = default;
	[[nodiscard]] static Builder builder() {
		return Builder {};
	}

private:
};

class users_health_response {
public:
	class Builder {
	public:
		[[nodiscard]] users_health_response build() && {
			users_health_response out;
			return out;
		}

		[[nodiscard]] users_health_response build() const & {
			users_health_response out;
			return out;
		}

	private:
	};

	users_health_response() = default;
	static constexpr unsigned status_code = 204;
	[[nodiscard]] static Builder builder() {
		return Builder {};
	}

private:
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
	static constexpr auto fields = std::make_tuple(
	    make_required_json_field(
	        "name",
	        static_cast<generated_api::users_create_user_request_body &(
	            generated_api::users_create_user_request_body::*)(std::string)>(
	            &generated_api::users_create_user_request_body::set_name),
	        static_cast<const std::string &(generated_api::users_create_user_request_body::*)() const & noexcept>(
	            &generated_api::users_create_user_request_body::name),
	        static_cast<std::string && (generated_api::users_create_user_request_body::*)() && noexcept>(
	            &generated_api::users_create_user_request_body::name)),
	    make_optional_json_field(
	        "nickname",
	        static_cast<generated_api::users_create_user_request_body &(
	            generated_api::users_create_user_request_body::*)(std::optional<std::string>)>(
	            &generated_api::users_create_user_request_body::set_nickname),
	        static_cast<const std::optional<std::string> &(generated_api::users_create_user_request_body::*)() const &
	                    noexcept>(&generated_api::users_create_user_request_body::nickname),
	        static_cast<std::optional<std::string> && (generated_api::users_create_user_request_body::*)() && noexcept>(
	            &generated_api::users_create_user_request_body::nickname)));
};

template <>
struct json_object_contract<generated_api::users_create_user_response_body> {
	static constexpr std::string_view type_name = "users_create_user_response_body";
	static constexpr auto fields = std::make_tuple(
	    make_required_json_field(
	        "id",
	        static_cast<generated_api::users_create_user_response_body &(
	            generated_api::users_create_user_response_body::*)(std::int64_t)>(
	            &generated_api::users_create_user_response_body::set_id),
	        static_cast<const std::int64_t &(generated_api::users_create_user_response_body::*)() const & noexcept>(
	            &generated_api::users_create_user_response_body::id),
	        static_cast<std::int64_t &&(generated_api::users_create_user_response_body::*)() && noexcept>(
	            &generated_api::users_create_user_response_body::id)),
	    make_required_json_field(
	        "active",
	        static_cast<generated_api::users_create_user_response_body &(
	            generated_api::users_create_user_response_body::*)(bool)>(
	            &generated_api::users_create_user_response_body::set_active),
	        static_cast<const bool &(generated_api::users_create_user_response_body::*)() const & noexcept>(
	            &generated_api::users_create_user_response_body::active),
	        static_cast<bool &&(generated_api::users_create_user_response_body::*)() && noexcept>(
	            &generated_api::users_create_user_response_body::active)));
};

} // namespace warp::codegen
