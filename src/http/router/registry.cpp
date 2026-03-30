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

namespace {

async_handler wrap_sync_handler(handler callback) {
	return [callback = std::move(callback)](request &&req) -> boost::asio::awaitable<response> {
		co_return callback(req);
	};
}

} // namespace

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

void registry::add(method verb, std::string path, async_handler h) {
	auto segments = compile_pattern(path);
	auto &root = method_roots_[verb];
	auto *current = &root;

	std::vector<route_parameter> parameters;
	parameters.reserve(segments.size());

	for (std::size_t i = 0; i < segments.size(); ++i) {
		const auto &segment = segments[i];
		if (segment.type == segment::kind::literal) {
			auto [it, inserted] = current->literal_children.try_emplace(segment.value, std::make_unique<node>());
			boost::ignore_unused(inserted);
			current = it->second.get();
			continue;
		}

		if (!current->parameter_child) {
			current->parameter_child = std::make_unique<node>();
		}
		current = current->parameter_child.get();
		parameters.push_back(route_parameter {.index = i, .name = segment.value});
	}

	if (!current->route) {
		current->route = std::make_unique<route_entry>();
	}

	current->route->handler = std::move(h);
	current->route->parameters = std::move(parameters);
}

void registry::add(method verb, std::string path, handler h) {
	add(verb, std::move(path), wrap_sync_handler(std::move(h)));
}

const async_handler *registry::find(request &req) const {
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

std::vector<registry::segment> registry::compile_pattern(const std::string &pattern) {
	if (pattern.empty() || pattern.front() != '/') {
		throw std::invalid_argument("route pattern must start with '/'");
	}
	if (pattern == "/") {
		return {};
	}

	std::vector<segment> segments;
	std::size_t start = 1;
	while (start <= pattern.size()) {
		auto end = pattern.find('/', start);
		auto token = pattern.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (token.empty()) {
			throw std::invalid_argument("route pattern contains empty segment");
		}
		if (token.front() == '{' && token.back() == '}') {
			if (token.size() <= 2) {
				throw std::invalid_argument("route parameter name cannot be empty");
			}
			std::string name = token.substr(1, token.size() - 2);
			segments.push_back(segment {segment::kind::parameter, std::move(name)});
		} else {
			segments.push_back(segment {segment::kind::literal, token});
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
	}
	return segments;
}

const registry::route_entry *registry::match_route(const node &root, std::string_view path) {
	auto clean_path = path.substr(0, path.find('?'));
	if (clean_path.empty()) {
		clean_path = "/";
	}
	if (clean_path.front() != '/') {
		return nullptr;
	}

	const node *current = &root;
	if (clean_path == "/") {
		return current->route.get();
	}

	std::size_t pos = 1;
	while (pos <= clean_path.size()) {
		const auto next = clean_path.find('/', pos);
		const auto len = next == std::string_view::npos ? clean_path.size() - pos : next - pos;
		if (len == 0) {
			return nullptr;
		}

		const auto token = clean_path.substr(pos, len);
		if (auto it = current->literal_children.find(token); it != current->literal_children.end()) {
			current = it->second.get();
		} else if (current->parameter_child) {
			current = current->parameter_child.get();
		} else {
			return nullptr;
		}

		if (next == std::string::npos) {
			break;
		}
		pos = next + 1;
	}

	return current->route.get();
}

void registry::apply_path_params(request &req, std::string_view path, const route_entry &route) {
	if (route.parameters.empty()) {
		return;
	}

	std::unordered_map<std::string, std::string> params;
	params.reserve(route.parameters.size());

	auto clean_path = path.substr(0, path.find('?'));
	if (clean_path.empty()) {
		clean_path = "/";
	}

	std::size_t parameter_index = 0;
	std::size_t segment_index = 0;
	std::size_t pos = 1;
	while (parameter_index < route.parameters.size() && pos <= clean_path.size()) {
		const auto next = clean_path.find('/', pos);
		const auto len = next == std::string_view::npos ? clean_path.size() - pos : next - pos;
		if (len == 0) {
			break;
		}

		if (route.parameters[parameter_index].index == segment_index) {
			params.emplace(route.parameters[parameter_index].name, std::string(clean_path.substr(pos, len)));
			++parameter_index;
		}

		if (next == std::string::npos) {
			break;
		}
		pos = next + 1;
		++segment_index;
	}

	req.set_path_params(std::move(params));
}

} // namespace warp::http
