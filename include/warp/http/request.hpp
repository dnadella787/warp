#pragma once

#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <boost/beast/http.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>

#include "warp/http/string_map.hpp"

namespace warp::http {

using beast_request = boost::beast::http::request<boost::beast::http::string_body>;

struct target_parse_error {
	std::string code;
	std::string message;
};

namespace detail {

inline int hex_value(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return 10 + (c - 'A');
	}
	if (c >= 'a' && c <= 'f') {
		return 10 + (c - 'a');
	}
	return -1;
}

inline std::optional<std::string> try_decode_url_component(std::string_view input, bool plus_as_space = false) {
	std::string output;
	output.reserve(input.size());
	for (std::size_t i = 0; i < input.size(); ++i) {
		const char c = input[i];
		if (c == '%') {
			if (i + 2 >= input.size()) {
				return std::nullopt;
			}
			const int hi = hex_value(input[i + 1]);
			const int lo = hex_value(input[i + 2]);
			if (hi < 0 || lo < 0) {
				return std::nullopt;
			}
			output.push_back(static_cast<char>((hi << 4) | lo));
			i += 2;
			continue;
		}
		if (c == '+' && plus_as_space) {
			output.push_back(' ');
			continue;
		}
		output.push_back(c);
	}
	return output;
}

inline std::optional<std::string> try_decode_query_component(std::string_view input) {
	return try_decode_url_component(input, true);
}

} // namespace detail

class request : public beast_request {
public:
	using parameter_map = transparent_string_map<std::string>;

	request();
	request(boost::beast::http::verb method, std::string_view target, unsigned version);
	request(const beast_request &other);
	request(beast_request &&other) noexcept;

	request &operator=(const beast_request &other);
	request &operator=(beast_request &&other) noexcept;

	void refresh_target_metadata();

	[[nodiscard]] std::string_view path() const noexcept;
	[[nodiscard]] const parameter_map &query_params() const noexcept;
	[[nodiscard]] std::optional<std::string_view> query_param(const char *key) const;
	[[nodiscard]] std::optional<std::string_view> query_param(const std::string &key) const;
	[[nodiscard]] std::optional<std::string_view> query_param(std::string_view key) const;
	[[nodiscard]] std::optional<std::string_view> header_param(const char *key) const;
	[[nodiscard]] std::optional<std::string_view> header_param(const std::string &key) const;
	[[nodiscard]] std::optional<std::string_view> header_param(std::string_view key) const;

	void set_path_params(parameter_map params);
	[[nodiscard]] const parameter_map &path_params() const noexcept;
	[[nodiscard]] std::optional<std::string_view> path_param(const char *key) const;
	[[nodiscard]] std::optional<std::string_view> path_param(const std::string &key) const;
	[[nodiscard]] std::optional<std::string_view> path_param(std::string_view key) const;
	[[nodiscard]] const std::optional<target_parse_error> &target_error() const noexcept;
	void set_target_error(target_parse_error error);
	void clear_target_error() noexcept;

	[[nodiscard]] boost::json::value json_body() const;
	[[nodiscard]] std::optional<boost::json::value> try_json_body() const noexcept;

private:
	void parse_target();

	std::string path_;
	parameter_map query_params_;
	parameter_map path_params_;
	std::optional<target_parse_error> target_error_;
};

inline request::request() {
	parse_target();
}

inline request::request(boost::beast::http::verb method, std::string_view target, unsigned version)
    : beast_request(method, target, version) {
	parse_target();
}

inline request::request(const beast_request &other) : beast_request(other) {
	parse_target();
}

inline request::request(beast_request &&other) noexcept : beast_request(std::move(other)) {
	parse_target();
}

inline request &request::operator=(const beast_request &other) {
	beast_request::operator=(other);
	parse_target();
	return *this;
}

inline request &request::operator=(beast_request &&other) noexcept {
	beast_request::operator=(std::move(other));
	parse_target();
	return *this;
}

inline void request::refresh_target_metadata() {
	parse_target();
}

inline std::string_view request::path() const noexcept {
	return path_;
}

inline const request::parameter_map &request::query_params() const noexcept {
	return query_params_;
}

inline std::optional<std::string_view> request::query_param(const char *key) const {
	return query_param(std::string_view {key});
}

inline std::optional<std::string_view> request::query_param(const std::string &key) const {
	if (auto it = query_params_.find(key); it != query_params_.end()) {
		return it->second;
	}
	return std::nullopt;
}

inline std::optional<std::string_view> request::query_param(std::string_view key) const {
	if (auto it = query_params_.find(key); it != query_params_.end()) {
		return it->second;
	}
	return std::nullopt;
}

inline std::optional<std::string_view> request::header_param(const char *key) const {
	return header_param(std::string_view {key});
}

inline std::optional<std::string_view> request::header_param(const std::string &key) const {
	return header_param(std::string_view {key});
}

inline std::optional<std::string_view> request::header_param(std::string_view key) const {
	if (auto it = find(key); it != end()) {
		return it->value();
	}
	return std::nullopt;
}

inline void request::set_path_params(parameter_map params) {
	path_params_ = std::move(params);
}

inline const request::parameter_map &request::path_params() const noexcept {
	return path_params_;
}

inline std::optional<std::string_view> request::path_param(const char *key) const {
	return path_param(std::string_view {key});
}

inline std::optional<std::string_view> request::path_param(const std::string &key) const {
	if (auto it = path_params_.find(key); it != path_params_.end()) {
		return it->second;
	}
	return std::nullopt;
}

inline std::optional<std::string_view> request::path_param(std::string_view key) const {
	if (auto it = path_params_.find(key); it != path_params_.end()) {
		return it->second;
	}
	return std::nullopt;
}

inline const std::optional<target_parse_error> &request::target_error() const noexcept {
	return target_error_;
}

inline void request::set_target_error(target_parse_error error) {
	if (!target_error_.has_value()) {
		target_error_ = std::move(error);
	}
}

inline void request::clear_target_error() noexcept {
	target_error_.reset();
}

inline boost::json::value request::json_body() const {
	return boost::json::parse(body());
}

inline std::optional<boost::json::value> request::try_json_body() const noexcept {
	try {
		return json_body();
	} catch (...) {
		return std::nullopt;
	}
}

inline void request::parse_target() {
	query_params_.clear();
	path_params_.clear();
	target_error_.reset();
	path_.assign(target());

	const std::string_view target_view = target();
	const auto query_pos = target_view.find('?');
	if (query_pos == std::string_view::npos) {
		return;
	}

	path_.assign(target_view.substr(0, query_pos));
	const std::string_view query = target_view.substr(query_pos + 1);
	std::size_t start = 0;
	while (start < query.size()) {
		const auto end = query.find('&', start);
		const auto token = query.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!token.empty()) {
			const auto eq = token.find('=');
			const auto key = detail::try_decode_query_component(token.substr(0, eq));
			if (!key.has_value()) {
				query_params_.clear();
				target_error_ = target_parse_error {
				    .code = "malformed_query_parameter",
				    .message = "malformed percent-encoding in query parameter name",
				};
				return;
			}
			const auto value = eq == std::string_view::npos ? std::optional<std::string>(std::string {})
			                                                : detail::try_decode_query_component(token.substr(eq + 1));
			if (!value.has_value()) {
				query_params_.clear();
				target_error_ = target_parse_error {
				    .code = "malformed_query_parameter",
				    .message = "malformed percent-encoding in query parameter '" + *key + "'",
				};
				return;
			}
			if (!key->empty()) {
				if (!query_params_.emplace(*key, *value).second) {
					query_params_.clear();
					target_error_ = target_parse_error {
					    .code = "duplicate_query_parameter",
					    .message = "duplicate query parameter '" + *key + "'",
					};
					return;
				}
			}
		}
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}
}

} // namespace warp::http
