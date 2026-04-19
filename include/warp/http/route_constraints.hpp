#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>

#include "../../../src/http/router/route_pattern.hpp"
#include "http.hpp"

namespace warp::http {

template <std::size_t N>
struct fixed_string {
	char value[N] {};

	constexpr fixed_string(const char (&literal)[N]) {
		for (std::size_t i = 0; i < N; ++i) {
			value[i] = literal[i];
		}
	}

	[[nodiscard]] constexpr std::size_t size() const noexcept {
		return N > 0 ? N - 1 : 0;
	}

	[[nodiscard]] constexpr std::string_view view() const noexcept {
		return {value, size()};
	}

	constexpr operator std::string_view() const noexcept {
		return view();
	}

	constexpr auto operator<=>(const fixed_string &) const = default;
};

template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

enum class query_constraint_presence {
	required,
	optional,
	forbidden,
};

struct query_constraint_descriptor {
	std::string_view name;
	query_constraint_presence presence {query_constraint_presence::required};
	bool has_exact_value {};
	std::string_view exact_value;
};

template <typename T>
concept query_constraint = requires {
	{ T::descriptor() } -> std::same_as<query_constraint_descriptor>;
};

namespace detail {

enum class query_constraint_name_error {
	none,
	empty_name,
	contains_separator,
};

template <route_pattern_validation_error Error>
consteval void fail_route_pattern_validation() {
	if constexpr (Error == route_pattern_validation_error::empty_path) {
		static_assert(Error != Error, "route path must not be empty");
	} else if constexpr (Error == route_pattern_validation_error::missing_leading_slash) {
		static_assert(Error != Error, "route path must start with '/'");
	} else if constexpr (Error == route_pattern_validation_error::contains_fragment) {
		static_assert(Error != Error, "route path must not contain a fragment");
	} else if constexpr (Error == route_pattern_validation_error::contains_query_string) {
		static_assert(Error != Error, "route pattern must not contain a query string");
	} else if constexpr (Error == route_pattern_validation_error::empty_segment) {
		static_assert(Error != Error, "route path contains an empty segment");
	} else if constexpr (Error == route_pattern_validation_error::malformed_parameter_segment) {
		static_assert(Error != Error, "route parameter segments must use the form '{name}'");
	} else if constexpr (Error == route_pattern_validation_error::empty_parameter_name) {
		static_assert(Error != Error, "route parameter name cannot be empty");
	} else if constexpr (Error == route_pattern_validation_error::parameter_name_contains_braces) {
		static_assert(Error != Error, "route parameter name cannot contain braces");
	} else if constexpr (Error == route_pattern_validation_error::duplicate_parameter_name) {
		static_assert(Error != Error, "route parameter names must be unique within a path");
	}
}

template <fixed_string Path>
consteval route_pattern_validation_result checked_route_path() {
	constexpr auto validation = validate_route_pattern(Path.view());
	if constexpr (validation.error != route_pattern_validation_error::none) {
		fail_route_pattern_validation<validation.error>();
	}
	return validation;
}

[[nodiscard]] constexpr query_constraint_name_error validate_query_constraint_name(std::string_view name) noexcept {
	if (name.empty()) {
		return query_constraint_name_error::empty_name;
	}
	for (const auto c : name) {
		if (c == '&' || c == '=' || c == '?' || c == '#' || c == '!' || c == '~') {
			return query_constraint_name_error::contains_separator;
		}
	}
	return query_constraint_name_error::none;
}

template <query_constraint_name_error Error>
consteval void fail_query_constraint_name_validation() {
	if constexpr (Error == query_constraint_name_error::empty_name) {
		static_assert(Error != Error, "query constraint name must not be empty");
	} else if constexpr (Error == query_constraint_name_error::contains_separator) {
		static_assert(Error != Error, "query constraint name must not contain '&', '=', '?', '#', '!', or '~'");
	}
}

template <fixed_string Name>
consteval void checked_query_constraint_name() {
	constexpr auto validation = validate_query_constraint_name(Name.view());
	if constexpr (validation != query_constraint_name_error::none) {
		fail_query_constraint_name_validation<validation>();
	}
}

template <query_constraint... QueryConstraints>
[[nodiscard]] consteval bool has_duplicate_query_constraints() {
	constexpr std::array<query_constraint_descriptor, sizeof...(QueryConstraints)> descriptors {
	    QueryConstraints::descriptor()...};
	for (std::size_t i = 0; i < descriptors.size(); ++i) {
		for (std::size_t j = i + 1; j < descriptors.size(); ++j) {
			if (descriptors[i].name == descriptors[j].name) {
				return true;
			}
		}
	}
	return false;
}

[[nodiscard]] constexpr bool route_shape_matches(std::string_view lhs, std::string_view rhs) noexcept {
	if (lhs == "/" || rhs == "/") {
		return lhs == rhs;
	}

	std::size_t lhs_offset = 1;
	std::size_t rhs_offset = 1;
	for (;;) {
		const auto lhs_segment = warp::http::detail::route_segment_at(lhs, lhs_offset);
		const auto rhs_segment = warp::http::detail::route_segment_at(rhs, rhs_offset);
		if (lhs_segment.empty() || rhs_segment.empty()) {
			return lhs_segment.empty() && rhs_segment.empty();
		}

		const auto lhs_parameter = warp::http::detail::is_parameter_segment(lhs_segment);
		const auto rhs_parameter = warp::http::detail::is_parameter_segment(rhs_segment);
		if (lhs_parameter != rhs_parameter) {
			return false;
		}
		if (!lhs_parameter && lhs_segment != rhs_segment) {
			return false;
		}

		const auto lhs_end = lhs.find('/', lhs_offset);
		const auto rhs_end = rhs.find('/', rhs_offset);
		if ((lhs_end == std::string_view::npos) != (rhs_end == std::string_view::npos)) {
			return false;
		}
		if (lhs_end == std::string_view::npos) {
			return true;
		}
		lhs_offset = lhs_end + 1;
		rhs_offset = rhs_end + 1;
	}
}

[[nodiscard]] constexpr bool descriptors_can_overlap(query_constraint_descriptor lhs,
                                                     query_constraint_descriptor rhs) noexcept {
	if (lhs.presence == query_constraint_presence::forbidden && rhs.presence == query_constraint_presence::forbidden) {
		return true;
	}
	if (lhs.presence == query_constraint_presence::forbidden) {
		return rhs.presence != query_constraint_presence::required;
	}
	if (rhs.presence == query_constraint_presence::forbidden) {
		return lhs.presence != query_constraint_presence::required;
	}
	if (lhs.presence != query_constraint_presence::required && rhs.presence != query_constraint_presence::required) {
		return true;
	}
	if (!lhs.has_exact_value || !rhs.has_exact_value) {
		return true;
	}
	return lhs.exact_value == rhs.exact_value;
}

template <typename Spec>
concept route_registration_spec_impl = requires {
	{ Spec::verb } -> std::convertible_to<method>;
	{ Spec::path_view() } -> std::same_as<std::string_view>;
	{ Spec::query_constraints };
	{ Spec::base_specificity } -> std::convertible_to<std::size_t>;
	{ Spec::query_constraint_count } -> std::convertible_to<std::size_t>;
	{ Spec::required_exact_constraints } -> std::convertible_to<std::size_t>;
	{ Spec::exact_constraint_count } -> std::convertible_to<std::size_t>;
};

template <route_registration_spec_impl Lhs, route_registration_spec_impl Rhs>
[[nodiscard]] consteval bool route_specs_are_ambiguous() {
	if (Lhs::verb != Rhs::verb) {
		return false;
	}
	if (!route_shape_matches(Lhs::path_view(), Rhs::path_view())) {
		return false;
	}
	for (const auto &lhs : Lhs::query_constraints) {
		for (const auto &rhs : Rhs::query_constraints) {
			if (lhs.name == rhs.name && !descriptors_can_overlap(lhs, rhs)) {
				return false;
			}
		}
	}

	const auto match_counts_overlap =
	    !(Lhs::query_constraint_count < Rhs::base_specificity || Rhs::query_constraint_count < Lhs::base_specificity);
	const auto exact_counts_overlap = !(Lhs::exact_constraint_count < Rhs::required_exact_constraints ||
	                                    Rhs::exact_constraint_count < Lhs::required_exact_constraints);
	return match_counts_overlap && exact_counts_overlap;
}

template <route_registration_spec_impl... Specs>
struct deterministic_route_definitions_helper {
	static constexpr bool value = true;
};

template <route_registration_spec_impl Head, route_registration_spec_impl... Tail>
struct deterministic_route_definitions_helper<Head, Tail...> {
	static constexpr bool value =
	    ((!route_specs_are_ambiguous<Head, Tail>()) && ...) && deterministic_route_definitions_helper<Tail...>::value;
};

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

template <fixed_string Name, query_constraint_presence Presence, fixed_string Value = "">
struct basic_query_constraint {
private:
	static constexpr bool validated_ = []() consteval {
		detail::checked_query_constraint_name<Name>();
		return true;
	}();

public:
	static constexpr auto literal = Name;
	static constexpr auto presence = Presence;
	static constexpr bool has_exact_value = Value.view().size() > 0;

	[[nodiscard]] static constexpr std::string_view name_view() noexcept {
		static_cast<void>(validated_);
		return Name.view();
	}

	[[nodiscard]] static constexpr std::string_view value_view() noexcept {
		return Value.view();
	}

	[[nodiscard]] static constexpr query_constraint_descriptor descriptor() noexcept {
		static_cast<void>(validated_);
		return {
		    .name = Name.view(), .presence = Presence, .has_exact_value = has_exact_value, .exact_value = Value.view()};
	}
};

template <fixed_string Name>
using required_query = basic_query_constraint<Name, query_constraint_presence::required>;

template <fixed_string Name>
using optional_query = basic_query_constraint<Name, query_constraint_presence::optional>;

template <fixed_string Name>
using forbidden_query = basic_query_constraint<Name, query_constraint_presence::forbidden>;

template <fixed_string Name, fixed_string Value>
using required_query_value = basic_query_constraint<Name, query_constraint_presence::required, Value>;

template <fixed_string Name, fixed_string Value>
using optional_query_value = basic_query_constraint<Name, query_constraint_presence::optional, Value>;

template <method Verb, fixed_string Path, query_constraint... QueryConstraints>
struct route_spec {
private:
	static constexpr bool validated_ = []() consteval {
		static_cast<void>(route_path<Path>::segment_count);
		return true;
	}();
	static_assert(!detail::has_duplicate_query_constraints<QueryConstraints...>(),
	              "route query constraint names must be unique");

public:
	static constexpr method verb = Verb;
	using route_path_type = route_path<Path>;
	static constexpr std::size_t query_constraint_count = sizeof...(QueryConstraints);
	static constexpr std::array<query_constraint_descriptor, sizeof...(QueryConstraints)> query_constraints {
	    QueryConstraints::descriptor()...};
	static constexpr std::size_t base_specificity =
	    ((QueryConstraints::descriptor().presence == query_constraint_presence::optional ? 0U : 1U) + ... + 0U);
	static constexpr std::size_t exact_constraint_count =
	    ((QueryConstraints::descriptor().has_exact_value ? 1U : 0U) + ... + 0U);
	static constexpr std::size_t required_exact_constraints =
	    (((QueryConstraints::descriptor().presence == query_constraint_presence::optional ||
	       !QueryConstraints::descriptor().has_exact_value)
	          ? 0U
	          : 1U) +
	     ... + 0U);

	[[nodiscard]] static constexpr std::string_view path_view() noexcept {
		static_cast<void>(validated_);
		return Path.view();
	}
};

template <typename T>
concept route_registration_spec = detail::route_registration_spec_impl<T>;

template <route_registration_spec... Specs>
[[nodiscard]] consteval bool deterministic_route_definitions() {
	return detail::deterministic_route_definitions_helper<Specs...>::value;
}

} // namespace warp::http
