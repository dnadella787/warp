#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "warp/net/http/common.hpp"

namespace warp::net::http {

class response {
public:
	response() = default;

	static response ok(std::string body);
	static response not_found();
	static response server_error(std::string message);

	response &set_header(std::string key, std::string value);

	void set_keep_alive(bool keep_alive) noexcept;

	[[nodiscard]] std::string_view body() const noexcept;
	[[nodiscard]] const headers &header_map() const noexcept;
	[[nodiscard]] unsigned status() const noexcept;
	[[nodiscard]] unsigned version() const noexcept;
	[[nodiscard]] unsigned keep_alive() const noexcept;
private:
	response(unsigned status, std::string body);

	unsigned status_ {200};
	std::string body_;
	headers headers_;
	unsigned version_ {11}; // Default HTTP version is 1.1 = (version/10).(version%10)
	bool keep_alive_;
};

inline response::response(unsigned status, std::string body) : status_(status), body_(std::move(body)), keep_alive_(true) {
}

inline response response::ok(std::string body) {
	return {200, std::move(body)};
}

inline response response::not_found() {
	response r(404, "Not Found");
	r.set_header("Content-Type", "text/plain");
	return r;
}

inline response response::server_error(std::string message) {
	response r(500, std::move(message));
	r.set_header("Content-Type", "text/plain");
	return r;
}

inline response &response::set_header(std::string key, std::string value) {
	headers_[std::move(key)] = std::move(value);
	return *this;
}

inline void response::set_keep_alive(bool keep_alive) noexcept {
	keep_alive_ = keep_alive;
}

inline std::string_view response::body() const noexcept {
	return body_;
}

inline const headers &response::header_map() const noexcept {
	return headers_;
}

inline unsigned response::status() const noexcept {
	return status_;
}

inline unsigned response::version() const noexcept {
	return version_;
}

inline unsigned response::keep_alive() const noexcept {
	return keep_alive_;
}
} // namespace warp::net::http
