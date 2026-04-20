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

enum class route_pattern_validation_error {
	none,
	empty_path,
	missing_leading_slash,
	contains_fragment,
	contains_query_string,
	empty_segment,
	malformed_parameter_segment,
	empty_parameter_name,
	parameter_name_contains_braces,
	duplicate_parameter_name,
};

struct route_pattern_validation_result {
	route_pattern_validation_error error {route_pattern_validation_error::none};
	std::size_t segment_count {};
	std::size_t parameter_count {};

	[[nodiscard]] constexpr bool ok() const noexcept {
		return error == route_pattern_validation_error::none;
	}
};

[[nodiscard]] constexpr std::string_view
route_pattern_validation_message(route_pattern_validation_error error) noexcept {
	switch (error) {
	case route_pattern_validation_error::none:
		return "";
	case route_pattern_validation_error::empty_path:
		return "route path must not be empty";
	case route_pattern_validation_error::missing_leading_slash:
		return "route path must start with '/'";
	case route_pattern_validation_error::contains_fragment:
		return "route path must not contain a fragment";
	case route_pattern_validation_error::contains_query_string:
		return "route pattern must not contain a query string";
	case route_pattern_validation_error::empty_segment:
		return "route path contains an empty segment";
	case route_pattern_validation_error::malformed_parameter_segment:
		return "route parameter segments must use the form '{name}'";
	case route_pattern_validation_error::empty_parameter_name:
		return "route parameter name cannot be empty";
	case route_pattern_validation_error::parameter_name_contains_braces:
		return "route parameter name cannot contain braces";
	case route_pattern_validation_error::duplicate_parameter_name:
		return "route parameter names must be unique within a path";
	}
	return "invalid route pattern";
}

namespace detail {

[[nodiscard]] constexpr std::string_view route_segment_at(std::string_view path, std::size_t start) noexcept {
	if (start >= path.size()) {
		return {};
	}
	const auto end = path.find('/', start);
	return end == std::string_view::npos ? path.substr(start) : path.substr(start, end - start);
}

[[nodiscard]] constexpr bool is_parameter_segment(std::string_view segment) noexcept {
	return !segment.empty() && (segment.front() == '{' || segment.back() == '}');
}

[[nodiscard]] constexpr std::string_view route_parameter_name(std::string_view segment) noexcept {
	return segment.substr(1, segment.size() - 2);
}

} // namespace detail

[[nodiscard]] constexpr route_pattern_validation_result validate_route_pattern(std::string_view pattern) noexcept {
	if (pattern.find('?') != std::string_view::npos) {
		return {.error = route_pattern_validation_error::contains_query_string};
	}
	if (pattern.empty()) {
		return {.error = route_pattern_validation_error::empty_path};
	}
	if (pattern.front() != '/') {
		return {.error = route_pattern_validation_error::missing_leading_slash};
	}
	if (pattern.find('#') != std::string_view::npos) {
		return {.error = route_pattern_validation_error::contains_fragment};
	}
	if (pattern == "/") {
		return {};
	}

	route_pattern_validation_result result;
	for (std::size_t start = 1; start <= pattern.size();) {
		const auto segment = detail::route_segment_at(pattern, start);
		if (segment.empty()) {
			return {.error = route_pattern_validation_error::empty_segment};
		}

		++result.segment_count;
		if (detail::is_parameter_segment(segment)) {
			if (segment.size() < 3 || segment.front() != '{' || segment.back() != '}') {
				return {
				    .error = route_pattern_validation_error::malformed_parameter_segment,
				    .segment_count = result.segment_count,
				    .parameter_count = result.parameter_count,
				};
			}

			const auto name = detail::route_parameter_name(segment);
			if (name.empty()) {
				return {
				    .error = route_pattern_validation_error::empty_parameter_name,
				    .segment_count = result.segment_count,
				    .parameter_count = result.parameter_count,
				};
			}
			if (name.find('{') != std::string_view::npos || name.find('}') != std::string_view::npos) {
				return {
				    .error = route_pattern_validation_error::parameter_name_contains_braces,
				    .segment_count = result.segment_count,
				    .parameter_count = result.parameter_count,
				};
			}

			for (std::size_t previous = 1; previous < start;) {
				const auto prior_segment = detail::route_segment_at(pattern, previous);
				if (detail::is_parameter_segment(prior_segment) &&
				    detail::route_parameter_name(prior_segment) == name) {
					return {
					    .error = route_pattern_validation_error::duplicate_parameter_name,
					    .segment_count = result.segment_count,
					    .parameter_count = result.parameter_count,
					};
				}
				const auto previous_end = pattern.find('/', previous);
				if (previous_end == std::string_view::npos) {
					break;
				}
				previous = previous_end + 1;
			}

			++result.parameter_count;
		}

		const auto end = pattern.find('/', start);
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	return result;
}

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

inline std::vector<std::string_view> split_route_path_views(std::string_view path) {
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

	std::vector<std::string_view> segments;
	segments.reserve(6);
	for (std::size_t start = 1; start <= clean.size();) {
		const auto segment = detail::route_segment_at(clean, start);
		if (segment.empty()) {
			throw std::invalid_argument("route path contains an empty segment");
		}

		segments.push_back(segment);

		const auto end = clean.find('/', start);
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}
	return segments;
}

inline std::vector<std::string> split_route_path(std::string_view path) {
	const auto raw_segments = split_route_path_views(path);
	std::vector<std::string> segments;
	segments.reserve(raw_segments.size());
	for (const auto segment : raw_segments) {
		segments.emplace_back(segment);
	}
	return segments;
}

inline route_pattern parse_route_pattern(std::string_view pattern) {
	const auto validation = validate_route_pattern(pattern);
	if (!validation.ok()) {
		throw std::invalid_argument(std::string(route_pattern_validation_message(validation.error)));
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
