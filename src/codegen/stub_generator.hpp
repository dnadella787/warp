#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "data_class_emitter.hpp"
#include "resource_stub_emitter.hpp"
#include "codegen/spec_parser.hpp"

namespace warp::codegen {

struct generated_artifact {
	std::string path;
	std::string content;
};

struct generated_stub_set {
	generated_artifact data_header;
	generated_artifact resource_header;
};

struct stub_generator_options {
	std::string namespace_name;
	std::string data_header_name {"generated_api_types.hpp"};
	std::string resource_header_name {"generated_api_resources.hpp"};
};

class stub_generator {
public:
	[[nodiscard]] generated_stub_set generate(const spec_ast &spec, const stub_generator_options &options = {}) const;
	[[nodiscard]] generated_stub_set generate(const api_model &model, const stub_generator_options &options = {}) const;
	[[nodiscard]] generated_stub_set generate_from_yaml(std::string_view yaml_text,
	                                                    const stub_generator_options &options = {}) const;
	[[nodiscard]] generated_stub_set generate_from_file(const std::filesystem::path &yaml_path,
	                                                    const stub_generator_options &options = {}) const;
};

} // namespace warp::codegen
