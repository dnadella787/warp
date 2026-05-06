#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "route_executor_table.hpp"
#include "warp/server/detail/route_definition.hpp"
#include "server/router/registry.hpp"

namespace warp::server {

template <typename Session>
class route_runtime {
public:
	route_runtime() = default;

	explicit route_runtime(std::vector<detail::route_definition> route_definitions)
	    : executors_(route_definitions.size()) {
		for (auto &definition : route_definitions) {
			const auto route_id = registry_.add(definition.verb, definition.path, definition.typed_query_constraints);
			executors_.set(route_id, std::move(definition.callback));
		}
	}

	[[nodiscard]] std::optional<registry::route_match> find(http::request &req) const {
		return registry_.find(req);
	}

	void dispatch(registry::route_id id, Session &session, std::size_t sequence, http::request request) const {
		executors_.dispatch(id, session, sequence, std::move(request));
	}

private:
	registry registry_;
	route_executor_table<Session> executors_;
};

} // namespace warp::server
