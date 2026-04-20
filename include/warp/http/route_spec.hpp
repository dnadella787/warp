#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>

#include "../../../src/http/router/route_pattern.hpp"

#include "http.hpp"
#include "query_constraints.hpp"
#include "route_path.hpp"

namespace warp::http {

namespace detail {

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
