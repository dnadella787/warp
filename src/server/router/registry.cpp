#include "registry.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace warp::server {

namespace {

void normalize_compiled_query_constraints(std::vector<http::compiled_query_constraint> &constraints) {
	http::detail::sort_compiled_query_constraints(constraints);
	for (std::size_t i = 1; i < constraints.size(); ++i) {
		if (constraints[i - 1].name == constraints[i].name) {
			throw std::invalid_argument("route query constraint names must be unique");
		}
	}
}

http::compiled_route compile_typed_route(http::method verb, std::string_view path,
                                         const std::vector<http::query_constraint_descriptor> &query_constraints) {
	http::compiled_route route {
	    .verb = verb,
	    .pattern = http::parse_route_pattern(path),
	};
	route.query_constraints.reserve(query_constraints.size());
	for (const auto &descriptor : query_constraints) {
		http::compiled_query_constraint constraint {
		    .name = std::string(descriptor.name),
		    .presence = descriptor.presence,
		};
		if (descriptor.exact_value.has_value()) {
			constraint.value = std::string(*descriptor.exact_value);
		}
		route.query_constraints.push_back(std::move(constraint));
	}
	normalize_compiled_query_constraints(route.query_constraints);
	return route;
}

} // namespace

registry::registry(const registry &other) : next_route_id_(other.next_route_id_) {
	method_roots_.reserve(other.method_roots_.size());
	for (const auto &[verb, root] : other.method_roots_) {
		method_roots_.emplace(verb, clone_node(root));
	}
}

registry &registry::operator=(const registry &other) {
	if (this == &other) {
		return *this;
	}

	method_roots_.clear();
	method_roots_.reserve(other.method_roots_.size());
	for (const auto &[verb, root] : other.method_roots_) {
		method_roots_.emplace(verb, clone_node(root));
	}
	next_route_id_ = other.next_route_id_;
	return *this;
}

registry::route_id registry::add(http::method verb, std::string path) {
	return add_route(verb, std::move(path));
}

registry::route_id registry::add_route(http::method verb, std::string path) {
	return add_compiled(parse_registered_route(verb, path));
}

registry::route_id registry::add_typed(http::method verb, std::string path,
                                       const std::vector<http::query_constraint_descriptor> &query_constraints) {
	return add_compiled(compile_typed_route(verb, path, query_constraints));
}

registry::route_id registry::add_compiled(http::compiled_route route) {
	normalize_compiled_query_constraints(route.query_constraints);
	auto &root = method_roots_[route.verb];
	auto *current = &root;
	const route_id id {next_route_id_++};

	std::vector<route_parameter> parameters;
	parameters.reserve(route.pattern.segments.size());

	for (std::size_t i = 0; i < route.pattern.segments.size(); ++i) {
		const auto &segment = route.pattern.segments[i];
		if (segment.kind == http::route_segment_kind::literal) {
			auto [it, inserted] = current->literal_children.try_emplace(segment.text, std::make_unique<node>());
			boost::ignore_unused(inserted);
			current = it->second.get();
			continue;
		}

		if (!current->parameter_child) {
			current->parameter_child = std::make_unique<node>();
		}
		current = current->parameter_child.get();
		parameters.push_back(route_parameter {.index = i, .name = segment.text});
	}

	for (const auto &existing : current->routes) {
		if (existing.query_constraints == route.query_constraints) {
			throw std::invalid_argument("duplicate route pattern for method, normalized path shape, and query shape");
		}
	}

	current->routes.push_back(route_entry {
	    .id = id,
	    .parameters = std::move(parameters),
	    .query_constraints = std::move(route.query_constraints),
	});
	return id;
}

std::optional<registry::route_match> registry::find(http::request &req) const {
	req.set_path_params({});
	const auto it = method_roots_.find(req.method());
	if (it == method_roots_.end()) {
		return std::nullopt;
	}

	std::vector<std::string_view> segments;
	try {
		segments = http::split_route_path_views(req.path());
	} catch (const std::invalid_argument &) {
		return std::nullopt;
	}

	if (const auto *route = match_route(it->second, req, segments)) {
		apply_path_params(req, segments, *route);
		return route_match {.id = route->id};
	}

	return std::nullopt;
}

std::size_t registry::method_hash::operator()(http::method verb) const noexcept {
	return std::hash<unsigned> {}(static_cast<unsigned>(verb));
}

registry::node registry::clone_node(const node &source) {
	node copy;
	for (const auto &[literal, child] : source.literal_children) {
		copy.literal_children.emplace(literal, std::make_unique<node>(clone_node(*child)));
	}
	if (source.parameter_child) {
		copy.parameter_child = std::make_unique<node>(clone_node(*source.parameter_child));
	}
	copy.routes = source.routes;
	return copy;
}

http::compiled_route registry::parse_registered_route(http::method verb, std::string_view route) {
	http::compiled_route parsed {
	    .verb = verb,
	    .pattern = http::parse_route_pattern(http::strip_query_string(route)),
	};
	const auto query_pos = route.find('?');
	if (query_pos == std::string_view::npos)
		return parsed;

	const auto raw_query = route.substr(query_pos + 1);
	std::size_t start = 0;
	while (start < raw_query.size()) {
		const auto end = raw_query.find('&', start);
		const auto token =
		    raw_query.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!token.empty()) {
			const auto eq = token.find('=');
			auto key = http::try_decode_query_component(token.substr(0, eq));
			if (!key.has_value() || key->empty())
				throw std::invalid_argument("route query constraint names must be non-empty and valid");

			auto presence = http::query_constraint_presence::required;
			if (key->front() == '!') {
				presence = http::query_constraint_presence::forbidden;
				key->erase(key->begin());
			} else if (key->front() == '~') {
				presence = http::query_constraint_presence::optional;
				key->erase(key->begin());
			}
			if (key->empty()) {
				throw std::invalid_argument("route query constraint names must be non-empty and valid");
			}

			const auto raw_value = eq == std::string_view::npos
			                           ? std::optional<std::string> {}
			                           : http::try_decode_query_component(token.substr(eq + 1));
			if (eq != std::string_view::npos && !raw_value.has_value())
				throw std::invalid_argument("route query constraint values must use valid percent-encoding");

			parsed.query_constraints.push_back(http::compiled_query_constraint {
			    .name = std::move(*key),
			    .presence = presence,
			    .value = std::move(raw_value),
			});
		}

		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	normalize_compiled_query_constraints(parsed.query_constraints);

	return parsed;
}

const registry::route_entry *registry::match_route(const node &root, const http::request &req,
                                                   const std::vector<std::string_view> &segments,
                                                   std::size_t segment_index) {
	if (segment_index == segments.size()) {
		return match_leaf_routes(root, req);
	}

	const auto &token = segments[segment_index];
	if (auto it = root.literal_children.find(token); it != root.literal_children.end()) {
		if (const auto *literal_match = match_route(*it->second, req, segments, segment_index + 1)) {
			return literal_match;
		}
	}

	if (root.parameter_child) {
		return match_route(*root.parameter_child, req, segments, segment_index + 1);
	}

	return nullptr;
}

const registry::route_entry *registry::match_leaf_routes(const node &current, const http::request &req) {
	const route_entry *best = nullptr;
	http::routing_detail::query_constraint_match_score best_score {};
	for (const auto &route : current.routes) {
		const auto score = match_query_constraints(route, req);
		if (!score.has_value()) {
			continue;
		}
		if (best == nullptr || is_better_match(route, *score, *best, best_score)) {
			best = &route;
			best_score = *score;
		}
	}
	return best;
}

std::optional<http::routing_detail::query_constraint_match_score>
registry::match_query_constraints(const route_entry &route, const http::request &req) {
	http::routing_detail::query_constraint_match_score score;
	for (const auto &constraint : route.query_constraints) {
		const auto actual = req.query_param(constraint.name);
		if (constraint.presence == http::query_constraint_presence::forbidden) {
			if (actual.has_value()) {
				return std::nullopt;
			}
			++score.matched_constraints;
			continue;
		}

		if (!actual.has_value()) {
			if (constraint.presence == http::query_constraint_presence::required) {
				return std::nullopt;
			}
			continue;
		}

		if (constraint.value.has_value() && *actual != *constraint.value) {
			return std::nullopt;
		}

		++score.matched_constraints;
		if (constraint.value.has_value()) {
			++score.matched_exact_constraints;
		}
	}
	return score;
}

bool registry::is_better_match(const route_entry &candidate,
                               http::routing_detail::query_constraint_match_score candidate_score,
                               const route_entry &current_best,
                               http::routing_detail::query_constraint_match_score current_best_score) {
	if (candidate_score.matched_constraints != current_best_score.matched_constraints) {
		return candidate_score.matched_constraints > current_best_score.matched_constraints;
	}
	if (candidate_score.matched_exact_constraints != current_best_score.matched_exact_constraints) {
		return candidate_score.matched_exact_constraints > current_best_score.matched_exact_constraints;
	}
	return candidate.id.index() < current_best.id.index();
}

void registry::apply_path_params(http::request &req, const std::vector<std::string_view> &segments,
                                 const route_entry &route) {
	http::request::parameter_map params;
	params.reserve(route.parameters.size());

	if (!route.parameters.empty()) {
		for (const auto &parameter : route.parameters) {
			if (parameter.index >= segments.size()) {
				break;
			}
			const auto decoded = warp::http::try_decode_url_component(segments[parameter.index]);
			if (!decoded.has_value()) {
				req.set_target_error(warp::http::target_parse_error {
				    .code = "malformed_path_parameter",
				    .message = "malformed percent-encoding in path parameter '" + parameter.name + "'",
				});
				req.set_path_params({});
				return;
			}
			params.emplace(parameter.name, std::move(*decoded));
		}
	}

	req.set_path_params(std::move(params));
}

} // namespace warp::server
