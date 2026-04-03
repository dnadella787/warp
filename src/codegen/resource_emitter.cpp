#include "warp/codegen/resource_emitter.hpp"

namespace warp::codegen {

std::string resource_emitter::emit_header(const api_model &model, const resource_emitter_options &options) const {
	return resource_stub_emitter().emit_header(model, {.data_header_include = options.model_header_include,
	                                                   .include_data_header = options.include_model_header});
}

} // namespace warp::codegen
