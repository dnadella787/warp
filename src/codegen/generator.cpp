#include "warp/codegen/generator.hpp"

#include "codegen/stub_generator.hpp"

namespace warp::codegen {

generated_api_stub api_stub_generator::generate_from_yaml(std::string_view yaml_text,
                                                          const api_stub_generator_options &options) const {
	const auto generated =
	    stub_generator().generate_from_yaml(yaml_text, {.namespace_name = options.namespace_name,
	                                                    .data_header_name = options.model_header_name,
	                                                    .resource_header_name = options.resource_header_name});
	return generated_api_stub {
	    .model_header = generated.data_header.content,
	    .resource_header = generated.resource_header.content,
	};
}

generated_api_stub api_stub_generator::generate_from_file(const std::filesystem::path &yaml_path,
                                                          const api_stub_generator_options &options) const {
	const auto generated =
	    stub_generator().generate_from_file(yaml_path, {.namespace_name = options.namespace_name,
	                                                    .data_header_name = options.model_header_name,
	                                                    .resource_header_name = options.resource_header_name});
	return generated_api_stub {
	    .model_header = generated.data_header.content,
	    .resource_header = generated.resource_header.content,
	};
}

} // namespace warp::codegen
