#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "warp/http/http.hpp"
#include "warp/http/query_constraint_semantics.hpp"
#include "warp/http/string_map.hpp"

namespace warp::http {

class registry {
public:
	registry() = default;
	registry(const registry &other);
	registry &operator=(const registry &other);
	registry(registry &&) noexcept = default;
	registry &operator=(registry &&) noexcept = default;
	void add(method verb, std::string path, handler h);
	void add_route(method verb, std::string path, handler h);
	void add_compiled(compiled_route route, handler h);
	[[nodiscard]] const handler *find(request &req) const;

private:
	struct route_parameter {
		std::size_t index {};
		std::string name;
	};

	struct route_entry {
		handler handler;
		std::vector<route_parameter> parameters;
		std::vector<compiled_query_constraint> query_constraints;
		std::size_t registration_order {};
	};

	struct node {
		transparent_string_map<std::unique_ptr<node>> literal_children;
		std::unique_ptr<node> parameter_child;
		std::vector<route_entry> routes;
	};

	struct method_hash {
		[[nodiscard]] std::size_t operator()(method verb) const noexcept;
	};

	static node clone_node(const node &source);
	[[nodiscard]] static compiled_route parse_registered_route(method verb, std::string_view route);
	[[nodiscard]] static const route_entry *match_route(const node &root, const request &req,
	                                                    const std::vector<std::string_view> &segments,
	                                                    std::size_t segment_index = 0);
	[[nodiscard]] static const route_entry *match_leaf_routes(const node &current, const request &req);
	[[nodiscard]] static std::optional<detail::query_constraint_match_score>
	match_query_constraints(const route_entry &route, const request &req);
	[[nodiscard]] static bool is_better_match(const route_entry &candidate,
	                                          detail::query_constraint_match_score candidate_score,
	                                          const route_entry &current_best,
	                                          detail::query_constraint_match_score current_best_score);
	static void apply_path_params(request &req, const std::vector<std::string_view> &segments,
	                              const route_entry &route);

	std::unordered_map<method, node, method_hash> method_roots_;
	std::size_t next_registration_order_ {};
};

} // namespace warp::http
