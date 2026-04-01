#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <boost/beast/http.hpp>
#include <boost/json/value.hpp>
#include <boost/json/serialize.hpp>

#include "warp/http/body_builder.hpp"
#include "warp/http/response.hpp"

namespace warp::http {

class response_builder {
public:
	response_builder() = default;

	response_builder &status(unsigned code);
	response_builder &status(boost::beast::http::status code);
	response_builder &body(std::string value);
	response_builder &body(std::string_view value);
	response_builder &body(const char *value);
	response_builder &body(const body_builder &value);
	response_builder &body(boost::json::value value);
	response_builder &content_type(std::string value);
	response_builder &version(unsigned value);
	response_builder &keep_alive(bool value);
	response_builder &header(std::string key, std::string value);

	[[nodiscard]] response build() const;

private:
	boost::beast::http::status status_ {boost::beast::http::status::ok};
	std::string body_;
	std::string content_type_ {"application/json"};
	unsigned version_ {11};
	bool keep_alive_ {true};
	boost::beast::http::fields headers_;
};

inline response_builder &response_builder::status(unsigned code) {
	status_ = static_cast<boost::beast::http::status>(code);
	return *this;
}

inline response_builder &response_builder::status(boost::beast::http::status code) {
	status_ = code;
	return *this;
}

inline response_builder &response_builder::body(std::string value) {
	body_ = std::move(value);
	return *this;
}

inline response_builder &response_builder::body(std::string_view value) {
	body_.assign(value);
	return *this;
}

inline response_builder &response_builder::body(const char *value) {
	body_ = value == nullptr ? std::string {} : std::string {value};
	return *this;
}

inline response_builder &response_builder::body(const body_builder &value) {
	body_ = value.build();
	return *this;
}

inline response_builder &response_builder::body(boost::json::value value) {
	body_ = boost::json::serialize(value);
	content_type_ = "application/json";
	return *this;
}

inline response_builder &response_builder::content_type(std::string value) {
	content_type_ = std::move(value);
	return *this;
}

inline response_builder &response_builder::version(unsigned value) {
	version_ = value;
	return *this;
}

inline response_builder &response_builder::keep_alive(bool value) {
	keep_alive_ = value;
	return *this;
}

inline response_builder &response_builder::header(std::string key, std::string value) {
	headers_.set(boost::beast::string_view {key}, boost::beast::string_view {value});
	return *this;
}

inline response response_builder::build() const {
	response resp {status_, version_};
	resp.body() = body_;
	resp.keep_alive(keep_alive_);
	resp.set(boost::beast::http::field::content_type, content_type_.empty() ? "application/json" : content_type_);
	for (const auto &field : headers_) {
		if (field.name() == boost::beast::http::field::unknown) {
			resp.insert(boost::beast::http::field::unknown, field.name_string(), field.value());
		} else {
			resp.set(field.name(), field.value());
		}
	}
	resp.prepare_payload();
	return resp;
}

} // namespace warp::http
