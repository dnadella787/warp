#pragma once

#include <string>

#include "warp/codegen/spec_model.hpp"

namespace warp::codegen {

struct data_class_emitter_options {
	std::string namespace_name;
};

class data_class_emitter {
public:
	[[nodiscard]] std::string emit_header(const api_spec &spec, const data_class_emitter_options &options = {}) const;
};

} // namespace warp::codegen
