#pragma once

#include <cstddef>
#include <string_view>

#include "route_fixed_string.hpp"

namespace warp::http {

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
		const auto segment = route_segment_at(pattern, start);
		if (segment.empty()) {
			return {.error = route_pattern_validation_error::empty_segment};
		}

		++result.segment_count;
		if (is_parameter_segment(segment)) {
			if (segment.size() < 3 || segment.front() != '{' || segment.back() != '}') {
				return {
				    .error = route_pattern_validation_error::malformed_parameter_segment,
				    .segment_count = result.segment_count,
				    .parameter_count = result.parameter_count,
				};
			}

			const auto name = route_parameter_name(segment);
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
				const auto prior_segment = route_segment_at(pattern, previous);
				if (is_parameter_segment(prior_segment) && route_parameter_name(prior_segment) == name) {
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

template <route_pattern_validation_error Error>
consteval void fail_route_pattern_validation() {
	static_assert(Error != route_pattern_validation_error::empty_path, "route path must not be empty");
	static_assert(Error != route_pattern_validation_error::missing_leading_slash, "route path must start with '/'");
	static_assert(Error != route_pattern_validation_error::contains_fragment, "route path must not contain a fragment");
	static_assert(Error != route_pattern_validation_error::contains_query_string,
	              "route pattern must not contain a query string");
	static_assert(Error != route_pattern_validation_error::empty_segment, "route path contains an empty segment");
	static_assert(Error != route_pattern_validation_error::malformed_parameter_segment,
	              "route parameter segments must use the form '{name}'");
	static_assert(Error != route_pattern_validation_error::empty_parameter_name,
	              "route parameter name cannot be empty");
	static_assert(Error != route_pattern_validation_error::parameter_name_contains_braces,
	              "route parameter name cannot contain braces");
	static_assert(Error != route_pattern_validation_error::duplicate_parameter_name,
	              "route parameter names must be unique within a path");
}

template <fixed_string Path>
consteval route_pattern_validation_result checked_route_path() {
	constexpr auto validation = detail::validate_route_pattern(Path.view());
	if constexpr (validation.error != route_pattern_validation_error::none)
		fail_route_pattern_validation<validation.error>();
	return validation;
}

} // namespace detail

[[nodiscard]] constexpr route_pattern_validation_result validate_route_pattern(std::string_view pattern) noexcept {
	return detail::validate_route_pattern(pattern);
}

template <fixed_string Path>
struct route_path {
private:
	static constexpr route_pattern_validation_result validation_ = detail::checked_route_path<Path>();

public:
	static constexpr auto literal = Path;
	static constexpr std::size_t segment_count = validation_.segment_count;
	static constexpr std::size_t parameter_count = validation_.parameter_count;

	[[nodiscard]] static constexpr std::string_view view() noexcept {
		return Path.view();
	}

	constexpr operator std::string_view() const noexcept {
		return view();
	}
};

} // namespace warp::http
