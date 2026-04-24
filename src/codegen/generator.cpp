#include "warp/codegen/generator.hpp"

namespace warp::codegen {

generated_api_stub api_stub_generator::generate(const spec_ast &spec, const api_stub_generator_options &options) const {
	return generate(build_api_model(spec, options.namespace_name), options);
}

generated_api_stub api_stub_generator::generate(const api_model &model,
                                                const api_stub_generator_options &options) const {
	const auto generated = stub_generator().generate(model, {.namespace_name = options.namespace_name,
	                                                         .data_header_name = options.model_header_name,
	                                                         .resource_header_name = options.resource_header_name});
	return generated_api_stub {
	    .model_header = generated.data_header.content,
	    .resource_header = generated.resource_header.content,
	};
}

generated_api_stub api_stub_generator::generate_from_yaml(std::string_view yaml_text,
                                                          const api_stub_generator_options &options) const {
	return generate(parse_spec_ast(yaml_text), options);
}

generated_api_stub api_stub_generator::generate_from_file(const std::filesystem::path &yaml_path,
                                                          const api_stub_generator_options &options) const {
	return generate(load_spec_ast(yaml_path), options);
}

} // namespace warp::codegen
