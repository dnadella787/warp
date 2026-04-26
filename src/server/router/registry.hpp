#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "warp/http/http.hpp"
#include "warp/server/router/query_constraint_semantics.hpp"
#include "warp/http/string_map.hpp"

namespace warp::server {

class registry {
public:
	registry() = default;
	registry(const registry &other);
	registry &operator=(const registry &other);
	registry(registry &&) noexcept = default;
	registry &operator=(registry &&) noexcept = default;
	void add(http::method verb, std::string path, http::handler h);
	void add_route(http::method verb, std::string path, http::handler h);
	void add_compiled(http::compiled_route route, http::handler h);
	[[nodiscard]] const http::handler *find(http::request &req) const;

private:
	struct route_parameter {
		std::size_t index {};
		std::string name;
	};

	struct route_entry {
		http::handler handler;
		std::vector<route_parameter> parameters;
		std::vector<http::compiled_query_constraint> query_constraints;
		std::size_t registration_order {};
	};

	struct node {
		http::transparent_string_map<std::unique_ptr<node>> literal_children;
		std::unique_ptr<node> parameter_child;
		std::vector<route_entry> routes;
	};

	struct method_hash {
		[[nodiscard]] std::size_t operator()(http::method verb) const noexcept;
	};

	static node clone_node(const node &source);
	[[nodiscard]] static http::compiled_route parse_registered_route(http::method verb, std::string_view route);
	[[nodiscard]] static const route_entry *match_route(const node &root, const http::request &req,
	                                                    const std::vector<std::string_view> &segments,
	                                                    std::size_t segment_index = 0);
	[[nodiscard]] static const route_entry *match_leaf_routes(const node &current, const http::request &req);
	[[nodiscard]] static std::optional<http::detail::query_constraint_match_score>
	match_query_constraints(const route_entry &route, const http::request &req);
	[[nodiscard]] static bool is_better_match(const route_entry &candidate,
	                                          http::detail::query_constraint_match_score candidate_score,
	                                          const route_entry &current_best,
	                                          http::detail::query_constraint_match_score current_best_score);
	static void apply_path_params(http::request &req, const std::vector<std::string_view> &segments,
	                              const route_entry &route);

	std::unordered_map<http::method, node, method_hash> method_roots_;
	std::size_t next_registration_order_ {};
};

} // namespace warp::server
