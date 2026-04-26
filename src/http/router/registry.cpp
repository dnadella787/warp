#include "registry.hpp"

#include <boost/asio/awaitable.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace warp::http {

namespace {

constexpr std::array<std::string_view, 4> route_priority_keys {
    "priority",
    "_priority",
    "__priority",
    "__warp_priority",
};

[[nodiscard]] bool is_priority_query_key(std::string_view key) noexcept {
	return std::ranges::find(route_priority_keys, key) != route_priority_keys.end();
}

[[nodiscard]] std::int64_t parse_route_priority(std::string_view route, std::string_view raw_value) {
	std::int64_t priority = 0;
	const auto [ptr, ec] = std::from_chars(raw_value.data(), raw_value.data() + raw_value.size(), priority);
	if (ec != std::errc {} || ptr != raw_value.data() + raw_value.size()) {
		throw std::invalid_argument("route priority must be a signed integer in route '" + std::string(route) + "'");
	}
	return priority;
}

void normalize_compiled_query_constraints(std::vector<compiled_query_constraint> &constraints) {
	detail::sort_compiled_query_constraints(constraints);
	for (std::size_t i = 1; i < constraints.size(); ++i) {
		if (constraints[i - 1].name == constraints[i].name) {
			throw std::invalid_argument("route query constraint names must be unique");
		}
	}
}

} // namespace

registry::registry(const registry &other) {
	next_registration_order_ = other.next_registration_order_;
	for (const auto &[verb, root] : other.method_roots_) {
		method_roots_.emplace(verb, clone_node(root));
	}
}

registry &registry::operator=(const registry &other) {
	if (this == &other) {
		return *this;
	}
	method_roots_.clear();
	next_registration_order_ = other.next_registration_order_;
	for (const auto &[verb, root] : other.method_roots_) {
		method_roots_.emplace(verb, clone_node(root));
	}
	return *this;
}

void registry::add(method verb, std::string path, handler h) {
	add_route(verb, std::move(path), std::move(h));
}

void registry::add_route(method verb, std::string path, handler h) {
	add_compiled(parse_registered_route(verb, path), std::move(h));
}

void registry::add_compiled(compiled_route route, handler h) {
	normalize_compiled_query_constraints(route.query_constraints);
	auto &root = method_roots_[route.verb];
	auto *current = &root;

	std::vector<route_parameter> parameters;
	parameters.reserve(route.pattern.segments.size());

	for (std::size_t i = 0; i < route.pattern.segments.size(); ++i) {
		const auto &segment = route.pattern.segments[i];
		if (segment.kind == route_segment_kind::literal) {
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
	    .handler = std::move(h),
	    .parameters = std::move(parameters),
	    .query_constraints = std::move(route.query_constraints),
	    .priority = route.priority,
	    .registration_order = next_registration_order_++,
	});
}

const handler *registry::find(request &req) const {
	req.set_path_params({});
	const auto it = method_roots_.find(req.method());
	if (it == method_roots_.end()) {
		return nullptr;
	}

	std::vector<std::string_view> segments;
	try {
		segments = split_route_path_views(req.path());
	} catch (const std::invalid_argument &) {
		return nullptr;
	}

	if (const auto *route = match_route(it->second, req, segments)) {
		apply_path_params(req, segments, *route);
		return &route->handler;
	}

	return nullptr;
}

std::size_t registry::method_hash::operator()(method verb) const noexcept {
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

compiled_route registry::parse_registered_route(method verb, std::string_view route) {
	compiled_route parsed {
	    .verb = verb,
	    .pattern = parse_route_pattern(strip_query_string(route)),
	};
	const auto query_pos = route.find('?');
	if (query_pos == std::string_view::npos)
		return parsed;

	const auto raw_query = route.substr(query_pos + 1);
	std::size_t start = 0;
	bool priority_set = false;
	while (start < raw_query.size()) {
		const auto end = raw_query.find('&', start);
		const auto token =
		    raw_query.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!token.empty()) {
			const auto eq = token.find('=');
			auto key = try_decode_query_component(token.substr(0, eq));
			if (!key.has_value() || key->empty())
				throw std::invalid_argument("route query constraint names must be non-empty and valid");

			auto presence = query_constraint_presence::required;
			if (key->front() == '!') {
				presence = query_constraint_presence::forbidden;
				key->erase(key->begin());
			} else if (key->front() == '~') {
				presence = query_constraint_presence::optional;
				key->erase(key->begin());
			}
			if (key->empty()) {
				throw std::invalid_argument("route query constraint names must be non-empty and valid");
			}

			const auto raw_value = eq == std::string_view::npos ? std::optional<std::string> {}
			                                                    : try_decode_query_component(token.substr(eq + 1));
			if (eq != std::string_view::npos && !raw_value.has_value())
				throw std::invalid_argument("route query constraint values must use valid percent-encoding");

			if (is_priority_query_key(*key)) {
				if (!raw_value.has_value())
					throw std::invalid_argument("route priority must be declared as key=value");
				if (priority_set)
					throw std::invalid_argument("route priority may only be declared once");
				parsed.priority = parse_route_priority(route, *raw_value);
				priority_set = true;
			} else {
				parsed.query_constraints.push_back(compiled_query_constraint {
				    .name = std::move(*key),
				    .presence = presence,
				    .value = std::move(raw_value),
				});
			}
		}

		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	normalize_compiled_query_constraints(parsed.query_constraints);

	return parsed;
}

const registry::route_entry *registry::match_route(const node &root, const request &req,
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

const registry::route_entry *registry::match_leaf_routes(const node &current, const request &req) {
	const route_entry *best = nullptr;
	detail::query_constraint_match_score best_score {};
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

std::optional<detail::query_constraint_match_score> registry::match_query_constraints(const route_entry &route,
                                                                                      const request &req) {
	detail::query_constraint_match_score score;
	for (const auto &constraint : route.query_constraints) {
		const auto actual = req.query_param(constraint.name);
		if (constraint.presence == query_constraint_presence::forbidden) {
			if (actual.has_value()) {
				return std::nullopt;
			}
			++score.matched_constraints;
			continue;
		}

		if (!actual.has_value()) {
			if (constraint.presence == query_constraint_presence::required) {
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

bool registry::is_better_match(const route_entry &candidate, detail::query_constraint_match_score candidate_score,
                               const route_entry &current_best,
                               detail::query_constraint_match_score current_best_score) {
	if (candidate_score.matched_constraints != current_best_score.matched_constraints) {
		return candidate_score.matched_constraints > current_best_score.matched_constraints;
	}
	if (candidate_score.matched_exact_constraints != current_best_score.matched_exact_constraints) {
		return candidate_score.matched_exact_constraints > current_best_score.matched_exact_constraints;
	}
	if (candidate.priority != current_best.priority) {
		return candidate.priority > current_best.priority;
	}
	return candidate.registration_order < current_best.registration_order;
}

void registry::apply_path_params(request &req, const std::vector<std::string_view> &segments,
                                 const route_entry &route) {
	request::parameter_map params;
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

} // namespace warp::http
