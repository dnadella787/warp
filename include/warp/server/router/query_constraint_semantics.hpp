#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "compiled_route.hpp"

namespace warp::http::detail {

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

[[nodiscard]] constexpr std::string_view constraint_name(query_constraint_descriptor constraint) noexcept {
	return constraint.name;
}

[[nodiscard]] inline std::string_view constraint_name(const compiled_query_constraint &constraint) noexcept {
	return constraint.name;
}

[[nodiscard]] constexpr query_constraint_presence constraint_presence(query_constraint_descriptor constraint) noexcept {
	return constraint.presence;
}

[[nodiscard]] inline query_constraint_presence
constraint_presence(const compiled_query_constraint &constraint) noexcept {
	return constraint.presence;
}

[[nodiscard]] constexpr bool constraint_has_exact_value(query_constraint_descriptor constraint) noexcept {
	return constraint.has_exact_value;
}

[[nodiscard]] inline bool constraint_has_exact_value(const compiled_query_constraint &constraint) noexcept {
	return constraint.value.has_value();
}

[[nodiscard]] constexpr std::string_view constraint_exact_value(query_constraint_descriptor constraint) noexcept {
	return constraint.exact_value;
}

[[nodiscard]] inline std::string_view constraint_exact_value(const compiled_query_constraint &constraint) noexcept {
	return constraint.value.has_value() ? std::string_view(*constraint.value) : std::string_view {};
}

template <typename Constraint>
[[nodiscard]] constexpr bool query_constraints_can_overlap(const Constraint &lhs, const Constraint &rhs) noexcept {
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

template <typename Constraint>
[[nodiscard]] constexpr bool
query_constraint_accepts_state(const std::optional<Constraint> &constraint, std::string_view lhs_exact_value,
                               std::string_view rhs_exact_value, query_value_state state) noexcept {
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

template <typename Constraint>
[[nodiscard]] constexpr query_constraint_match_score query_constraint_score(const std::optional<Constraint> &constraint,
                                                                            query_value_state state) noexcept {
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

inline void sort_query_constraint_descriptors(std::vector<query_constraint_descriptor> &constraints) {
	std::ranges::sort(constraints, [](query_constraint_descriptor lhs, query_constraint_descriptor rhs) {
		return lhs.name < rhs.name;
	});
}

} // namespace warp::http::detail
