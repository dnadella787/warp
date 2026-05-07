#pragma once

#include <string_view>

#include "codegen/model.hpp"

namespace warp::codegen::detail {

class validation_rule_normalizer {
public:
	[[nodiscard]] validation_rules normalize(const validation_rule_spec &input, value_kind kind,
	                                         source_span fallback_span, std::string_view subject) const;
};

} // namespace warp::codegen::detail
