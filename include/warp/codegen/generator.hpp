#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace warp::codegen {

struct generated_api_stub {
	std::string model_header;
	std::string resource_header;
};

struct api_stub_generator_options {
	std::string namespace_name;
	std::string model_header_name {"generated_api_types.hpp"};
	std::string resource_header_name {"generated_api_resources.hpp"};
};

class api_stub_generator {
public:
	[[nodiscard]] generated_api_stub generate_from_yaml(std::string_view yaml_text,
	                                                    const api_stub_generator_options &options = {}) const;
	[[nodiscard]] generated_api_stub generate_from_file(const std::filesystem::path &yaml_path,
	                                                    const api_stub_generator_options &options = {}) const;
};

} // namespace warp::codegen
