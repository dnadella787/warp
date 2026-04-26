#pragma once

#include <string>

#include "codegen/model.hpp"

namespace warp::codegen {

class data_class_emitter {
public:
	[[nodiscard]] std::string emit_header(const api_model &model) const;
};

} // namespace warp::codegen
