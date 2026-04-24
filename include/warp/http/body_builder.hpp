#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>

namespace warp::http {

template <typename T>
concept json_convertible = requires(T &&t) { boost::json::value_from(std::forward<T>(t)); };

class body_builder {
public:
	body_builder() = default;

	body_builder &set(std::string_view key, std::string_view value);

	template <json_convertible J>
	body_builder &set(std::string_view key, J &&value) {
		body_.insert_or_assign(key, std::forward<J>(value));
		return *this;
	}

	[[nodiscard]] std::string build() const;
	[[nodiscard]] const boost::json::object &json() const noexcept;

private:
	boost::json::object body_;
};

inline body_builder &body_builder::set(std::string_view key, std::string_view value) {
	body_.insert_or_assign(key, value);
	return *this;
}

inline std::string body_builder::build() const {
	return boost::json::serialize(body_);
}

inline const boost::json::object &body_builder::json() const noexcept {
	return body_;
}

} // namespace warp::http