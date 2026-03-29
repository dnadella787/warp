#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>

namespace warp::http {

class body_builder {
public:
	body_builder() = default;

	body_builder &set(std::string key, std::string value);
	body_builder &set(std::string key, std::string_view value);
	body_builder &set(std::string key, const char *value);
	body_builder &set(std::string key, boost::json::value value);

	template <typename T>
	body_builder &set(std::string key, T &&value) {
		body_[std::move(key)] = boost::json::value_from(std::forward<T>(value));
		return *this;
	}

	[[nodiscard]] std::string build() const;
	[[nodiscard]] const boost::json::object &json() const noexcept;

private:
	boost::json::object body_;
};

inline body_builder &body_builder::set(std::string key, std::string value) {
	body_[std::move(key)] = std::move(value);
	return *this;
}

inline body_builder &body_builder::set(std::string key, std::string_view value) {
	body_[std::move(key)] = value;
	return *this;
}

inline body_builder &body_builder::set(std::string key, const char *value) {
	body_[std::move(key)] = value;
	return *this;
}

inline body_builder &body_builder::set(std::string key, boost::json::value value) {
	body_[std::move(key)] = std::move(value);
	return *this;
}

inline std::string body_builder::build() const {
	return boost::json::serialize(body_);
}

inline const boost::json::object &body_builder::json() const noexcept {
	return body_;
}

} // namespace warp::http