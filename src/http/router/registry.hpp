#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "warp/http/http.hpp"

namespace warp::http {

using handler = std::variant<sync_handler, async_handler>;

class registry {
public:
	registry() = default;
	registry(const registry &other);
	registry &operator=(const registry &other);
	registry(registry &&) noexcept = default;
	registry &operator=(registry &&) noexcept = default;
	void add(method verb, std::string path, async_handler h);
	void add(method verb, std::string path, sync_handler h);
	void add_route(method verb, std::string path, handler h);
	[[nodiscard]] const handler *find(request &req) const;

private:
	struct route_parameter {
		std::size_t index {};
		std::string name;
	};

	struct route_entry {
		handler handler;
		std::vector<route_parameter> parameters;
	};

	struct transparent_string_hash {
		using is_transparent = void;

		[[nodiscard]] std::size_t operator()(std::string_view value) const noexcept;
		[[nodiscard]] std::size_t operator()(const std::string &value) const noexcept;
	};

	struct transparent_string_equal {
		using is_transparent = void;

		[[nodiscard]] bool operator()(const std::string &lhs, const std::string &rhs) const noexcept;
		[[nodiscard]] bool operator()(std::string_view lhs, std::string_view rhs) const noexcept;
		[[nodiscard]] bool operator()(const std::string &lhs, std::string_view rhs) const noexcept;
		[[nodiscard]] bool operator()(std::string_view lhs, const std::string &rhs) const noexcept;
	};

	struct node {
		std::unordered_map<std::string, std::unique_ptr<node>, transparent_string_hash, transparent_string_equal>
		    literal_children;
		std::unique_ptr<node> parameter_child;
		std::unique_ptr<route_entry> route;
	};

	struct method_hash {
		[[nodiscard]] std::size_t operator()(method verb) const noexcept;
	};

	static node clone_node(const node &source);
	[[nodiscard]] static const route_entry *match_route(const node &root, std::string_view path);
	static void apply_path_params(request &req, std::string_view path, const route_entry &route);

	std::unordered_map<method, node, method_hash> method_roots_;
};

} // namespace warp::http
