#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace warp::http {

enum class route_segment_kind {
	literal,
	parameter,
};

struct route_segment {
	route_segment_kind kind {route_segment_kind::literal};
	std::string text;
};

struct route_pattern {
	std::string original_path;
	std::vector<route_segment> segments;
	std::string shape_key {"/"};
};

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

inline std::string decode_url_component(std::string_view input, bool plus_as_space = false) {
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
		} else if (c == '+' && plus_as_space) {
			output.push_back(' ');
		} else {
			output.push_back(c);
		}
	}
	return output;
}

inline std::optional<std::string> try_decode_query_component(std::string_view input) {
	return try_decode_url_component(input, true);
}

inline std::string decode_query_component(std::string_view input) {
	return decode_url_component(input, true);
}

inline std::string_view strip_query_string(std::string_view target) {
	return target.substr(0, target.find('?'));
}

inline std::vector<std::string> split_route_path(std::string_view path) {
	if (path.empty()) {
		throw std::invalid_argument("route path must not be empty");
	}
	if (path.front() != '/') {
		throw std::invalid_argument("route path must start with '/'");
	}
	if (path.find('#') != std::string_view::npos) {
		throw std::invalid_argument("route path must not contain a fragment");
	}

	const auto clean = strip_query_string(path);
	if (clean.empty()) {
		throw std::invalid_argument("route path must not be empty");
	}
	if (clean == "/") {
		return {};
	}

	std::vector<std::string> segments;
	std::size_t start = 1;
	while (start <= clean.size()) {
		const auto end = clean.find('/', start);
		const auto token = clean.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (token.empty()) {
			throw std::invalid_argument("route path contains an empty segment");
		}
		segments.emplace_back(token);
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	return segments;
}

inline route_pattern parse_route_pattern(std::string_view pattern) {
	if (pattern.find('?') != std::string_view::npos) {
		throw std::invalid_argument("route pattern must not contain a query string");
	}

	route_pattern parsed;
	parsed.original_path = std::string(pattern);
	const auto raw_segments = split_route_path(pattern);
	std::unordered_set<std::string> parameter_names;

	if (raw_segments.empty()) {
		parsed.shape_key = "/";
		return parsed;
	}

	std::string shape_key;
	for (const auto &segment_text : raw_segments) {
		shape_key.push_back('/');
		if (segment_text.front() == '{' || segment_text.back() == '}') {
			if (segment_text.size() < 3 || segment_text.front() != '{' || segment_text.back() != '}') {
				throw std::invalid_argument("route parameter segments must use the form '{name}'");
			}
			const auto name = segment_text.substr(1, segment_text.size() - 2);
			if (name.empty()) {
				throw std::invalid_argument("route parameter name cannot be empty");
			}
			if (name.find('{') != std::string::npos || name.find('}') != std::string::npos) {
				throw std::invalid_argument("route parameter name cannot contain braces");
			}
			if (!parameter_names.insert(name).second) {
				throw std::invalid_argument("route parameter names must be unique within a path");
			}
			parsed.segments.push_back(route_segment {.kind = route_segment_kind::parameter, .text = name});
			shape_key += "{}";
			continue;
		}

		parsed.segments.push_back(route_segment {.kind = route_segment_kind::literal, .text = segment_text});
		shape_key += segment_text;
	}

	parsed.shape_key = std::move(shape_key);
	return parsed;
}

} // namespace warp::http
