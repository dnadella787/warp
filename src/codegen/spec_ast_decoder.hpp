#pragma once

#include "codegen/spec_model.hpp"
#include "codegen/yaml_document_parser.hpp"

namespace warp::codegen::detail {

[[nodiscard]] spec_ast decode_spec_ast(const yaml_node &document);

} // namespace warp::codegen::detail
