#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "codegen/diagnostics.hpp"

namespace warp::codegen::detail {

struct yaml_node {
	enum class kind {
		scalar,
		map,
		list,
	};

	kind type {kind::scalar};
	std::string scalar;
	std::vector<std::pair<std::string, yaml_node>> map_values;
	std::vector<yaml_node> list_values;
	std::size_t line {0};
	std::size_t column {0};
};

[[nodiscard]] source_span span_of(const yaml_node &node);
[[nodiscard]] yaml_node parse_yaml_document(std::string_view yaml_text);

} // namespace warp::codegen::detail
