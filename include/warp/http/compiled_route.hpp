#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "http.hpp"
#include "query_constraints.hpp"
#include "route_pattern.hpp"

namespace warp::http {

struct compiled_query_constraint {
	std::string name;
	query_constraint_presence presence {query_constraint_presence::required};
	std::optional<std::string> value;

	[[nodiscard]] bool operator==(const compiled_query_constraint &other) const = default;
};

struct compiled_route {
	method verb {};
	route_pattern pattern;
	std::vector<compiled_query_constraint> query_constraints;
};

namespace detail {

inline void sort_compiled_query_constraints(std::vector<compiled_query_constraint> &constraints) {
	std::ranges::sort(constraints, [](const compiled_query_constraint &lhs, const compiled_query_constraint &rhs) {
		return lhs.name < rhs.name;
	});
}

} // namespace detail

} // namespace warp::http
