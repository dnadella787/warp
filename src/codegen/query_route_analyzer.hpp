#pragma once

#include <optional>
#include <string>

#include "codegen/model.hpp"

namespace warp::codegen::detail {

class query_route_analyzer {
public:
	[[nodiscard]] std::optional<query_route_model>
	build_query_route(const request_model &request, const std::string &spec_name, source_span span) const;
	void validate_route_groups(resource_model &resource) const;
};

} // namespace warp::codegen::detail
