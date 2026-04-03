#pragma once

#include <string>

#include "warp/codegen/spec_model.hpp"

namespace warp::codegen {

struct resource_stub_emitter_options {
	std::string namespace_name;
	std::string data_header_include {"generated_api_types.hpp"};
	bool include_data_header {true};
};

class resource_stub_emitter {
public:
	[[nodiscard]] std::string emit_header(const api_spec &spec,
	                                      const resource_stub_emitter_options &options = {}) const;
};

} // namespace warp::codegen
