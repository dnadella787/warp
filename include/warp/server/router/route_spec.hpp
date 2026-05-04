#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string_view>

#include "warp/http/http.hpp"
#include "query_constraints.hpp"
#include "route_path.hpp"

namespace warp::http {

namespace detail {

struct query_constraint_match_score {
	std::size_t matched_constraints {};
	std::size_t matched_exact_constraints {};
};

enum class query_value_state {
	absent,
	lhs_exact,
	rhs_exact,
	other_present,
};

[[nodiscard]] constexpr query_constraint_presence constraint_presence(query_constraint_descriptor constraint) noexcept {
	return constraint.presence;
}

[[nodiscard]] constexpr bool constraint_has_exact_value(query_constraint_descriptor constraint) noexcept {
	return constraint.exact_value.has_value();
}

[[nodiscard]] constexpr std::string_view constraint_exact_value(query_constraint_descriptor constraint) noexcept {
	return constraint.exact_value.value_or(std::string_view {});
}

[[nodiscard]] constexpr bool query_constraints_can_overlap(query_constraint_descriptor lhs,
                                                           query_constraint_descriptor rhs) noexcept {
	if (constraint_presence(lhs) == query_constraint_presence::forbidden &&
	    constraint_presence(rhs) == query_constraint_presence::forbidden) {
		return true;
	}
	if (constraint_presence(lhs) == query_constraint_presence::forbidden) {
		return constraint_presence(rhs) != query_constraint_presence::required;
	}
	if (constraint_presence(rhs) == query_constraint_presence::forbidden) {
		return constraint_presence(lhs) != query_constraint_presence::required;
	}
	if (constraint_presence(lhs) != query_constraint_presence::required &&
	    constraint_presence(rhs) != query_constraint_presence::required) {
		return true;
	}
	if (!constraint_has_exact_value(lhs) || !constraint_has_exact_value(rhs)) {
		return true;
	}
	return constraint_exact_value(lhs) == constraint_exact_value(rhs);
}

[[nodiscard]] constexpr bool query_match_scores_equal(query_constraint_match_score lhs,
                                                      query_constraint_match_score rhs) noexcept {
	return lhs.matched_constraints == rhs.matched_constraints &&
	       lhs.matched_exact_constraints == rhs.matched_exact_constraints;
}

[[nodiscard]] constexpr bool exact_value_matches_state(std::string_view exact_value, std::string_view lhs_exact_value,
                                                       std::string_view rhs_exact_value,
                                                       query_value_state state) noexcept {
	switch (state) {
	case query_value_state::absent:
	case query_value_state::other_present:
		return false;
	case query_value_state::lhs_exact:
		return !lhs_exact_value.empty() && exact_value == lhs_exact_value;
	case query_value_state::rhs_exact:
		return !rhs_exact_value.empty() && exact_value == rhs_exact_value;
	}
	return false;
}

[[nodiscard]] constexpr bool
query_constraint_accepts_state(const std::optional<query_constraint_descriptor> &constraint,
                               std::string_view lhs_exact_value, std::string_view rhs_exact_value,
                               query_value_state state) noexcept {
	if (!constraint.has_value()) {
		return true;
	}
	if (constraint_presence(*constraint) == query_constraint_presence::forbidden) {
		return state == query_value_state::absent;
	}
	if (state == query_value_state::absent) {
		return constraint_presence(*constraint) == query_constraint_presence::optional;
	}
	if (!constraint_has_exact_value(*constraint)) {
		return true;
	}
	return exact_value_matches_state(constraint_exact_value(*constraint), lhs_exact_value, rhs_exact_value, state);
}

[[nodiscard]] constexpr query_constraint_match_score
query_constraint_score(const std::optional<query_constraint_descriptor> &constraint, query_value_state state) noexcept {
	if (!constraint.has_value()) {
		return {};
	}
	if (constraint_presence(*constraint) == query_constraint_presence::forbidden) {
		return {.matched_constraints = 1};
	}
	if (state == query_value_state::absent) {
		return {};
	}
	return {
	    .matched_constraints = 1,
	    .matched_exact_constraints = constraint_has_exact_value(*constraint) ? 1U : 0U,
	};
}

[[nodiscard]] constexpr query_constraint_match_score add_query_match_scores(query_constraint_match_score lhs,
                                                                            query_constraint_match_score rhs) noexcept {
	return {
	    .matched_constraints = lhs.matched_constraints + rhs.matched_constraints,
	    .matched_exact_constraints = lhs.matched_exact_constraints + rhs.matched_exact_constraints,
	};
}

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

		const auto lhs_parameter = is_possible_parameter_segment(lhs_segment);
		const auto rhs_parameter = is_possible_parameter_segment(rhs_segment);
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
	const auto lhs_exact_value =
	    lhs_descriptor.has_value() ? lhs_descriptor->exact_value.value_or(std::string_view {}) : std::string_view {};
	const auto rhs_exact_value =
	    rhs_descriptor.has_value() ? rhs_descriptor->exact_value.value_or(std::string_view {}) : std::string_view {};

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

/**
 * how to read:
 * - let U := typename std::remove_cvref_t<T>::value_type and U == query_constraint_descriptor
 * - std::tuple_size_v<T> (after stripping T down) must be well defined for T
 */
template <typename T>
concept query_constraint_descriptor_array = requires {
	typename std::remove_cvref_t<T>::value_type;
	std::tuple_size_v<std::remove_cvref_t<T>>;
} && std::same_as<typename std::remove_cvref_t<T>::value_type, query_constraint_descriptor>;

template <typename Spec>
concept route_registration_spec_impl = requires {
	{ Spec::verb } -> std::convertible_to<method>;
	{ Spec::path_view() } -> std::same_as<std::string_view>;
	requires query_constraint_descriptor_array<decltype(Spec::query_constraints)>;
	requires std::tuple_size_v<std::remove_cvref_t<decltype(Spec::query_constraints)>> == Spec::query_constraint_count;
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
	    ((QueryConstraints::descriptor().exact_value.has_value() ? 1U : 0U) + ... + 0U);
	static constexpr std::size_t required_exact_constraints =
	    (((QueryConstraints::descriptor().presence == query_constraint_presence::optional ||
	       !QueryConstraints::descriptor().exact_value.has_value())
	          ? 0U
	          : 1U) +
	     ... + 0U);

	[[nodiscard]] static constexpr std::string_view path_view() noexcept {
		static_cast<void>(validated_);
		return Path.view();
	}
};

// external wrapper
template <typename T>
concept route_registration_spec = detail::route_registration_spec_impl<T>;

template <route_registration_spec... Specs>
[[nodiscard]] consteval bool deterministic_route_definitions() {
	return detail::deterministic_route_definitions_helper<Specs...>::value;
}

} // namespace warp::http
