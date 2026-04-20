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

inline users_create_user_request_body tag_invoke(boost::json::value_to_tag<users_create_user_request_body>,
                                                 const boost::json::value &value) {
	const auto &obj = value.as_object();
	users_create_user_request_body out;
	const auto *raw_name = obj.if_contains("name");
	if (raw_name == nullptr) {
		throw std::invalid_argument("missing required field 'name' for users_create_user_request_body");
	}
	out.set_name(boost::json::value_to<std::string>(*raw_name));
	const auto *raw_nickname = obj.if_contains("nickname");
	if (raw_nickname != nullptr) {
		out.set_nickname(boost::json::value_to<std::string>(*raw_nickname));
	}
	return out;
}

inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value,
                       const users_create_user_request_body &input) {
	boost::json::object obj;
	obj["name"] = boost::json::value_from(input.name());
	if (input.nickname().has_value()) {
		obj["nickname"] = boost::json::value_from(*input.nickname());
	}
	value = std::move(obj);
}

inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value, users_create_user_request_body &&input) {
	boost::json::object obj;
	obj["name"] = boost::json::value_from(std::move(input).name());
	auto nickname_value = std::move(input).nickname();
	if (nickname_value.has_value()) {
		obj["nickname"] = boost::json::value_from(std::move(*nickname_value));
	}
	value = std::move(obj);
}

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

inline users_create_user_response_body tag_invoke(boost::json::value_to_tag<users_create_user_response_body>,
                                                  const boost::json::value &value) {
	const auto &obj = value.as_object();
	users_create_user_response_body out;
	const auto *raw_id = obj.if_contains("id");
	if (raw_id == nullptr) {
		throw std::invalid_argument("missing required field 'id' for users_create_user_response_body");
	}
	out.set_id(boost::json::value_to<std::int64_t>(*raw_id));
	const auto *raw_active = obj.if_contains("active");
	if (raw_active == nullptr) {
		throw std::invalid_argument("missing required field 'active' for users_create_user_response_body");
	}
	out.set_active(boost::json::value_to<bool>(*raw_active));
	return out;
}

inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value,
                       const users_create_user_response_body &input) {
	boost::json::object obj;
	obj["id"] = boost::json::value_from(input.id());
	obj["active"] = boost::json::value_from(input.active());
	value = std::move(obj);
}

inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value,
                       users_create_user_response_body &&input) {
	boost::json::object obj;
	obj["id"] = boost::json::value_from(std::move(input).id());
	obj["active"] = boost::json::value_from(std::move(input).active());
	value = std::move(obj);
}

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

} // namespace generated_api
