#pragma once

#include <string>

#include "warp/codegen/resource_stub_emitter.hpp"

namespace warp::codegen {

struct resource_emitter_options {
	std::string namespace_name;
	std::string model_header_include {"generated_api_types.hpp"};
	bool include_model_header {true};
};

class resource_emitter {
public:
	[[nodiscard]] std::string emit_header(const api_spec &spec, const resource_emitter_options &options = {}) const;
};

} // namespace warp::codegen
