#pragma once

#include <string>
#include <vector>

#include "warp/http/http.hpp"
#include "warp/server/router/query_constraints.hpp"

namespace warp::server::detail {

struct route_definition {
	http::method verb;
	std::string path;
	std::vector<http::query_constraint_descriptor> typed_query_constraints;
	http::handler callback;
};

} // namespace warp::server::detail
