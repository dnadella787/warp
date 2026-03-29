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

namespace warp::http {

using beast_request = boost::beast::http::request<boost::beast::http::string_body>;

class request : public beast_request {
public:
	request();
	request(boost::beast::http::verb method, std::string_view target, unsigned version);
	request(const beast_request &other);
	request(beast_request &&other) noexcept;

	request &operator=(const beast_request &other);
	request &operator=(beast_request &&other) noexcept;

	void refresh_target_metadata();

	[[nodiscard]] std::string_view path() const noexcept;
	[[nodiscard]] const std::unordered_map<std::string, std::string> &query_params() const noexcept;
	[[nodiscard]] std::optional<std::string_view> query_param(std::string_view key) const;

	void set_path_params(std::unordered_map<std::string, std::string> params);
	[[nodiscard]] const std::unordered_map<std::string, std::string> &path_params() const noexcept;
	[[nodiscard]] std::optional<std::string_view> path_param(std::string_view key) const;

	[[nodiscard]] boost::json::value json_body() const;
	[[nodiscard]] std::optional<boost::json::value> try_json_body() const noexcept;

private:
	static int hex_value(char c);
	static std::string decode_component(std::string_view input);
	void parse_target();

	std::string path_;
	std::unordered_map<std::string, std::string> query_params_;
	std::unordered_map<std::string, std::string> path_params_;
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

inline const std::unordered_map<std::string, std::string> &request::query_params() const noexcept {
	return query_params_;
}

inline std::optional<std::string_view> request::query_param(std::string_view key) const {
	if (auto it = query_params_.find(std::string(key)); it != query_params_.end()) {
		return it->second;
	}
	return std::nullopt;
}

inline void request::set_path_params(std::unordered_map<std::string, std::string> params) {
	path_params_ = std::move(params);
}

inline const std::unordered_map<std::string, std::string> &request::path_params() const noexcept {
	return path_params_;
}

inline std::optional<std::string_view> request::path_param(std::string_view key) const {
	if (auto it = path_params_.find(std::string(key)); it != path_params_.end()) {
		return it->second;
	}
	return std::nullopt;
}

inline boost::json::value request::json_body() const {
	return boost::json::parse(body());
}

inline std::optional<boost::json::value> request::try_json_body() const noexcept {
	try {
		return boost::json::parse(body());
	} catch (const std::exception &) {
		return std::nullopt;
	}
}

inline int request::hex_value(char c) {
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

inline std::string request::decode_component(std::string_view input) {
	std::string output;
	output.reserve(input.size());
	for (std::size_t i = 0; i < input.size(); ++i) {
		const char c = input[i];
		if (c == '%') {
			if (i + 2 < input.size()) {
				const int hi = hex_value(input[i + 1]);
				const int lo = hex_value(input[i + 2]);
				if (hi >= 0 && lo >= 0) {
					output.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
					continue;
				}
			}
			output.push_back(c);
		} else if (c == '+') {
			output.push_back(' ');
		} else {
			output.push_back(c);
		}
	}
	return output;
}

inline void request::parse_target() {
	query_params_.clear();
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
			const auto key = decode_component(token.substr(0, eq));
			const auto value = eq == std::string_view::npos ? std::string {} : decode_component(token.substr(eq + 1));
			if (!key.empty()) {
				query_params_[key] = value;
			}
		}
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}
}

} // namespace warp::http
