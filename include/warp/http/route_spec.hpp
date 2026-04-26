#pragma once

#include <array>
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "http.hpp"
#include "query_constraints.hpp"
#include "query_constraint_semantics.hpp"
#include "route_path.hpp"
#include "route_pattern.hpp"

namespace warp::http {

namespace detail {

[[nodiscard]] constexpr bool route_shape_matches(std::string_view lhs, std::string_view rhs) noexcept {
	if (lhs == "/" || rhs == "/") {
		return lhs == rhs;
	}

	std::size_t lhs_offset = 1;
	std::size_t rhs_offset = 1;
	for (;;) {
		const auto lhs_segment = route_segment_at(lhs, lhs_offset);
		const auto rhs_segment = route_segment_at(rhs, rhs_offset);
		if (lhs_segment.empty() || rhs_segment.empty()) {
			return lhs_segment.empty() && rhs_segment.empty();
		}

		const auto lhs_parameter = is_parameter_segment(lhs_segment);
		const auto rhs_parameter = is_parameter_segment(rhs_segment);
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

template <std::size_t Capacity>
struct constraint_name_set {
	std::array<std::string_view, Capacity> names {};
	std::size_t size {};
};

[[nodiscard]] constexpr bool constraint_name_set_contains(const auto &names, std::string_view name) noexcept {
	for (std::size_t i = 0; i < names.size; ++i) {
		if (names.names[i] == name) {
			return true;
		}
	}
	return false;
}

template <typename Spec>
[[nodiscard]] constexpr std::optional<query_constraint_descriptor>
find_query_constraint_descriptor(std::string_view name) noexcept {
	for (const auto &descriptor : Spec::query_constraints) {
		if (descriptor.name == name) {
			return descriptor;
		}
	}
	return std::nullopt;
}

template <typename Lhs, typename Rhs>
[[nodiscard]] constexpr auto combined_constraint_names() noexcept {
	constexpr std::size_t capacity = Lhs::query_constraint_count + Rhs::query_constraint_count;
	constraint_name_set<capacity> result;
	for (const auto &descriptor : Lhs::query_constraints) {
		result.names[result.size++] = descriptor.name;
	}
	for (const auto &descriptor : Rhs::query_constraints) {
		if (!constraint_name_set_contains(result, descriptor.name)) {
			result.names[result.size++] = descriptor.name;
		}
	}
	return result;
}

template <typename Lhs, typename Rhs, std::size_t Capacity>
[[nodiscard]] consteval bool route_specs_can_tie_on_score(const constraint_name_set<Capacity> &names, std::size_t index,
                                                          query_constraint_match_score lhs_score = {},
                                                          query_constraint_match_score rhs_score = {}) {
	if (index == names.size) {
		return query_match_scores_equal(lhs_score, rhs_score);
	}

	const auto lhs_descriptor = find_query_constraint_descriptor<Lhs>(names.names[index]);
	const auto rhs_descriptor = find_query_constraint_descriptor<Rhs>(names.names[index]);
	const auto lhs_exact_value = lhs_descriptor.has_value() && lhs_descriptor->has_exact_value
	                                 ? lhs_descriptor->exact_value
	                                 : std::string_view {};
	const auto rhs_exact_value = rhs_descriptor.has_value() && rhs_descriptor->has_exact_value
	                                 ? rhs_descriptor->exact_value
	                                 : std::string_view {};

	constexpr std::array<query_value_state, 4> states {
	    query_value_state::absent,
	    query_value_state::lhs_exact,
	    query_value_state::rhs_exact,
	    query_value_state::other_present,
	};
	for (const auto state : states) {
		if (!query_constraint_accepts_state(lhs_descriptor, lhs_exact_value, rhs_exact_value, state) ||
		    !query_constraint_accepts_state(rhs_descriptor, lhs_exact_value, rhs_exact_value, state)) {
			continue;
		}
		if (route_specs_can_tie_on_score<Lhs, Rhs>(
		        names, index + 1, add_query_match_scores(lhs_score, query_constraint_score(lhs_descriptor, state)),
		        add_query_match_scores(rhs_score, query_constraint_score(rhs_descriptor, state)))) {
			return true;
		}
	}
	return false;
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
			if (lhs.name == rhs.name && !query_constraints_can_overlap(lhs, rhs)) {
				return false;
			}
		}
	}
	return route_specs_can_tie_on_score<Lhs, Rhs>(combined_constraint_names<Lhs, Rhs>(), 0);
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

namespace detail {

template <route_registration_spec Spec>
[[nodiscard]] inline compiled_route compile_route_spec() {
	compiled_route route {
	    .verb = Spec::verb,
	    .pattern = parse_route_pattern(Spec::path_view()),
	};
	route.query_constraints.reserve(Spec::query_constraints.size());
	for (const auto &descriptor : Spec::query_constraints) {
		compiled_query_constraint constraint {
		    .name = std::string(descriptor.name),
		    .presence = descriptor.presence,
		};
		if (descriptor.has_exact_value) {
			constraint.value = std::string(descriptor.exact_value);
		}
		route.query_constraints.push_back(std::move(constraint));
	}
	sort_compiled_query_constraints(route.query_constraints);
	return route;
}

} // namespace detail

template <route_registration_spec... Specs>
[[nodiscard]] consteval bool deterministic_route_definitions() {
	return detail::deterministic_route_definitions_helper<Specs...>::value;
}

} // namespace warp::http
