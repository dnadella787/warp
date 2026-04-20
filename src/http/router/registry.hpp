#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "route_pattern.hpp"
#include "warp/http/http.hpp"
#include "warp/http/server_builder.hpp"

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
	[[nodiscard]] const handler *find(request &req) const;

private:
	struct route_parameter {
		std::size_t index {};
		std::string name;
	};

	struct query_constraint {
		std::string name;
		query_constraint_presence presence {query_constraint_presence::required};
		std::optional<std::string> value;

		[[nodiscard]] bool operator==(const query_constraint &other) const = default;
	};

	struct query_match_score {
		std::size_t matched_constraints {};
		std::size_t matched_exact_constraints {};
	};

	struct route_entry {
		handler handler;
		std::vector<route_parameter> parameters;
		std::vector<query_constraint> query_constraints;
		std::int64_t priority {};
		std::size_t registration_order {};
	};

	struct parsed_route {
		route_pattern pattern;
		std::vector<query_constraint> query_constraints;
		std::int64_t priority {};
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
		std::vector<route_entry> routes;
	};

	struct method_hash {
		[[nodiscard]] std::size_t operator()(method verb) const noexcept;
	};

	static node clone_node(const node &source);
	[[nodiscard]] static parsed_route parse_registered_route(std::string_view route);
	[[nodiscard]] static const route_entry *match_route(const node &root, const request &req,
	                                                    const std::vector<std::string_view> &segments,
	                                                    std::size_t segment_index = 0);
	[[nodiscard]] static const route_entry *match_leaf_routes(const node &current, const request &req);
	[[nodiscard]] static std::optional<query_match_score> match_query_constraints(const route_entry &route,
	                                                                              const request &req);
	[[nodiscard]] static bool is_better_match(const route_entry &candidate, query_match_score candidate_score,
	                                          const route_entry &current_best, query_match_score current_best_score);
	static void apply_path_params(request &req, const std::vector<std::string_view> &segments,
	                              const route_entry &route);

	std::unordered_map<method, node, method_hash> method_roots_;
	std::size_t next_registration_order_ {};
};

} // namespace warp::http
