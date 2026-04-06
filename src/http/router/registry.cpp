#include "registry.hpp"

#include <boost/asio/awaitable.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace warp::http {

registry::registry(const registry &other) {
	for (const auto &[verb, root] : other.method_roots_) {
		method_roots_.emplace(verb, clone_node(root));
	}
}

registry &registry::operator=(const registry &other) {
	if (this == &other) {
		return *this;
	}
	method_roots_.clear();
	for (const auto &[verb, root] : other.method_roots_) {
		method_roots_.emplace(verb, clone_node(root));
	}
	return *this;
}

void registry::add(method verb, std::string path, handler h) {
	add_route(verb, std::move(path), std::move(h));
}

void registry::add_route(method verb, std::string path, handler h) {
	const auto pattern = parse_route_pattern(path);
	auto &root = method_roots_[verb];
	auto *current = &root;

	std::vector<route_parameter> parameters;
	parameters.reserve(pattern.segments.size());

	for (std::size_t i = 0; i < pattern.segments.size(); ++i) {
		const auto &segment = pattern.segments[i];
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

	if (current->route) {
		throw std::invalid_argument("duplicate route pattern for method and normalized path shape");
	}

	current->route = std::make_unique<route_entry>();
	current->route->handler = std::move(h);
	current->route->parameters = std::move(parameters);
}

const handler *registry::find(request &req) const {
	req.set_path_params({});
	const auto it = method_roots_.find(req.method());
	if (it == method_roots_.end()) {
		return nullptr;
	}

	if (const auto *route = match_route(it->second, req.path())) {
		apply_path_params(req, req.path(), *route);
		return &route->handler;
	}

	return nullptr;
}

std::size_t registry::transparent_string_hash::operator()(std::string_view value) const noexcept {
	return std::hash<std::string_view> {}(value);
}

std::size_t registry::transparent_string_hash::operator()(const std::string &value) const noexcept {
	return (*this)(std::string_view {value});
}

bool registry::transparent_string_equal::operator()(const std::string &lhs, const std::string &rhs) const noexcept {
	return lhs == rhs;
}

bool registry::transparent_string_equal::operator()(std::string_view lhs, std::string_view rhs) const noexcept {
	return lhs == rhs;
}

bool registry::transparent_string_equal::operator()(const std::string &lhs, std::string_view rhs) const noexcept {
	return std::string_view {lhs} == rhs;
}

bool registry::transparent_string_equal::operator()(std::string_view lhs, const std::string &rhs) const noexcept {
	return lhs == std::string_view {rhs};
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
	if (source.route) {
		copy.route = std::make_unique<route_entry>(*source.route);
	}
	return copy;
}

const registry::route_entry *registry::match_route(const node &root, std::string_view path) {
	std::vector<std::string> segments;
	try {
		segments = warp::http::split_route_path(path);
	} catch (const std::invalid_argument &) {
		return nullptr;
	}

	const node *current = &root;
	if (segments.empty()) {
		return current->route.get();
	}

	for (const auto &token : segments) {
		if (auto it = current->literal_children.find(token); it != current->literal_children.end()) {
			current = it->second.get();
		} else if (current->parameter_child) {
			current = current->parameter_child.get();
		} else {
			return nullptr;
		}
	}

	return current->route.get();
}

void registry::apply_path_params(request &req, std::string_view path, const route_entry &route) {
	std::unordered_map<std::string, std::string> params;
	params.reserve(route.parameters.size());

	if (!route.parameters.empty()) {
		const auto segments = warp::http::split_route_path(path);
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
