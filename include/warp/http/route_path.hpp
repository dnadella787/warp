#pragma once

#include <string_view>

#include "route_fixed_string.hpp"
#include "route_pattern.hpp"

namespace warp::http {

namespace detail {

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
	constexpr auto validation = validate_route_pattern(Path.view());
	if constexpr (validation.error != route_pattern_validation_error::none)
		fail_route_pattern_validation<validation.error>();
	return validation;
}

} // namespace detail

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
