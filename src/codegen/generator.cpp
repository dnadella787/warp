#include "warp/codegen/generator.hpp"

#include <utility>

namespace warp::codegen {

generated_api_stub api_stub_generator::generate(const api_spec &spec, const api_stub_generator_options &options) const {
	const auto generated = stub_generator().generate(spec, {.namespace_name = options.namespace_name,
	                                                        .data_header_name = options.model_header_name,
	                                                        .resource_header_name = options.resource_header_name});
	return generated_api_stub {
	    .model_header = generated.data_header.content,
	    .resource_header = generated.resource_header.content,
	};
}

generated_api_stub api_stub_generator::generate_from_yaml(std::string_view yaml_text,
                                                          const api_stub_generator_options &options) const {
	return generate(parse_api_spec(yaml_text), options);
}

generated_api_stub api_stub_generator::generate_from_file(const std::filesystem::path &yaml_path,
                                                          const api_stub_generator_options &options) const {
	return generate(load_api_spec(yaml_path), options);
}

} // namespace warp::codegen
