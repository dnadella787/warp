#include "codegen/query_route_analyzer.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "codegen/model_diagnostics.hpp"
#include "server/router/query_constraint_semantics.hpp"

namespace warp::codegen::detail {

namespace {

[[nodiscard]] bool ordered_contains(const std::vector<std::string> &values, std::string_view value) {
	return std::find(values.begin(), values.end(), value) != values.end();
}

void append_unique(std::vector<std::string> &values, std::string_view value) {
	if (!ordered_contains(values, value)) {
		values.emplace_back(value);
	}
}

[[nodiscard]] std::optional<warp::http::compiled_query_constraint>
find_query_constraint(const std::vector<warp::http::compiled_query_constraint> &constraints, std::string_view name) {
	for (const auto &constraint : constraints) {
		if (constraint.name == name) {
			return constraint;
		}
	}
	return std::nullopt;
}

[[nodiscard]] bool
query_routes_can_tie_on_score(const std::vector<warp::http::compiled_query_constraint> &lhs,
                              const std::vector<warp::http::compiled_query_constraint> &rhs,
                              const std::vector<std::string_view> &constraint_names, std::size_t index = 0,
                              warp::http::routing_detail::query_constraint_match_score lhs_score = {},
                              warp::http::routing_detail::query_constraint_match_score rhs_score = {}) {
	if (index == constraint_names.size()) {
		return warp::http::routing_detail::query_match_scores_equal(lhs_score, rhs_score);
	}

	const auto lhs_descriptor = find_query_constraint(lhs, constraint_names[index]);
	const auto rhs_descriptor = find_query_constraint(rhs, constraint_names[index]);
	const auto lhs_exact_value = lhs_descriptor.has_value() && lhs_descriptor->value.has_value()
	                                 ? std::string_view(*lhs_descriptor->value)
	                                 : std::string_view {};
	const auto rhs_exact_value = rhs_descriptor.has_value() && rhs_descriptor->value.has_value()
	                                 ? std::string_view(*rhs_descriptor->value)
	                                 : std::string_view {};

	constexpr std::array<warp::http::routing_detail::query_value_state, 4> states {
	    warp::http::routing_detail::query_value_state::absent,
	    warp::http::routing_detail::query_value_state::lhs_exact,
	    warp::http::routing_detail::query_value_state::rhs_exact,
	    warp::http::routing_detail::query_value_state::other_present,
	};

	for (const auto state : states) {
		if (!warp::http::routing_detail::query_constraint_accepts_state(lhs_descriptor, lhs_exact_value,
		                                                                rhs_exact_value, state) ||
		    !warp::http::routing_detail::query_constraint_accepts_state(rhs_descriptor, lhs_exact_value,
		                                                                rhs_exact_value, state)) {
			continue;
		}

		if (query_routes_can_tie_on_score(
		        lhs, rhs, constraint_names, index + 1,
		        warp::http::routing_detail::add_query_match_scores(
		            lhs_score, warp::http::routing_detail::query_constraint_score(lhs_descriptor, state)),
		        warp::http::routing_detail::add_query_match_scores(
		            rhs_score, warp::http::routing_detail::query_constraint_score(rhs_descriptor, state)))) {
			return true;
		}
	}

	return false;
}

[[nodiscard]] std::vector<warp::http::compiled_query_constraint>
effective_query_route_constraints(const query_route_model &query_route, const route_group_model &group) {
	std::vector<warp::http::compiled_query_constraint> constraints;
	constraints.reserve(group.routing_query_parameters.size());
	for (const auto &name : group.routing_query_parameters) {
		if (const auto constraint = find_query_constraint(query_route.constraints, name); constraint.has_value()) {
			constraints.push_back(*constraint);
			continue;
		}
		constraints.push_back(warp::http::compiled_query_constraint {
		    .name = name,
		    .presence = warp::http::query_constraint_presence::forbidden,
		});
	}
	warp::http::detail::sort_compiled_query_constraints(constraints);
	return constraints;
}

[[nodiscard]] bool query_route_specs_overlap(const std::vector<warp::http::compiled_query_constraint> &lhs,
                                             const std::vector<warp::http::compiled_query_constraint> &rhs) {
	std::vector<std::string_view> constraint_names;
	constraint_names.reserve(lhs.size() + rhs.size());
	for (const auto &constraint : lhs) {
		if (std::find(constraint_names.begin(), constraint_names.end(), constraint.name) == constraint_names.end()) {
			constraint_names.push_back(constraint.name);
		}
	}
	for (const auto &constraint : rhs) {
		if (std::find(constraint_names.begin(), constraint_names.end(), constraint.name) == constraint_names.end()) {
			constraint_names.push_back(constraint.name);
		}
	}
	for (const auto &lhs_constraint : lhs) {
		for (const auto &rhs_constraint : rhs) {
			if (lhs_constraint.name == rhs_constraint.name &&
			    !warp::http::routing_detail::query_constraints_can_overlap(lhs_constraint, rhs_constraint)) {
				return false;
			}
		}
	}
	return query_routes_can_tie_on_score(lhs, rhs, constraint_names);
}

void validate_route_group(resource_model &resource, route_group_model &group) {
	group.query_route_endpoint_indices.clear();
	group.fallback_endpoint_index.reset();
	group.routing_query_parameters.clear();

	for (const auto endpoint_index : group.endpoint_indices) {
		const auto &endpoint = resource.endpoints.at(endpoint_index);
		if (endpoint.query_route.has_value()) {
			group.query_route_endpoint_indices.push_back(endpoint_index);
			for (const auto constraint : endpoint.query_route->constraints) {
				append_unique(group.routing_query_parameters, constraint.name);
			}
			continue;
		}

		if (group.fallback_endpoint_index.has_value()) {
			fail(endpoint.span, "model.duplicate_route",
			     "duplicate route '" + group.path + "' for method " + std::string(to_string(group.method)) +
			         " requires deterministic query constraints or a single fallback endpoint");
		}
		group.fallback_endpoint_index = endpoint_index;
	}

	if (group.query_route_endpoint_indices.empty()) {
		if (group.endpoint_indices.size() > 1) {
			fail(resource.endpoints.at(group.endpoint_indices.back()).span, "model.duplicate_route",
			     "duplicate route '" + group.path + "' for method " + std::string(to_string(group.method)) +
			         " is ambiguous without required query parameter constraints");
		}
		return;
	}

	for (std::size_t i = 0; i < group.query_route_endpoint_indices.size(); ++i) {
		const auto left_index = group.query_route_endpoint_indices[i];
		const auto &left_endpoint = resource.endpoints.at(left_index);
		const auto left_constraints = effective_query_route_constraints(*left_endpoint.query_route, group);
		for (std::size_t j = i + 1; j < group.query_route_endpoint_indices.size(); ++j) {
			const auto right_index = group.query_route_endpoint_indices[j];
			const auto &right_endpoint = resource.endpoints.at(right_index);
			const auto right_constraints = effective_query_route_constraints(*right_endpoint.query_route, group);
			if (query_route_specs_overlap(left_constraints, right_constraints)) {
				fail(right_endpoint.span, "model.ambiguous_query_route",
				     "query-aware routes '" + left_endpoint.endpoint_name + "' and '" + right_endpoint.endpoint_name +
				         "' for " + std::string(to_string(group.method)) + " " + group.path +
				         " accept overlapping query parameter sets");
			}
		}
	}
}

} // namespace

std::optional<query_route_model> query_route_analyzer::build_query_route(const request_model &request,
                                                                         const std::string &spec_name,
                                                                         source_span span) const {
	query_route_model query_route;
	query_route.span = span;
	query_route.spec_name = spec_name;
	bool has_required_constraint = false;

	for (const auto &parameter : request.parameters) {
		if (parameter.location != parameter_location::query) {
			continue;
		}
		query_route.constraints.push_back(warp::http::compiled_query_constraint {
		    .name = parameter.source_name,
		    .presence = parameter.required ? warp::http::query_constraint_presence::required
		                                   : warp::http::query_constraint_presence::optional,
		});
		has_required_constraint = has_required_constraint || parameter.required;
	}

	if (!has_required_constraint) {
		return std::nullopt;
	}
	warp::http::detail::sort_compiled_query_constraints(query_route.constraints);
	return query_route;
}

void query_route_analyzer::validate_route_groups(resource_model &resource) const {
	for (auto &group : resource.route_groups) {
		validate_route_group(resource, group);
	}
}

} // namespace warp::codegen::detail
