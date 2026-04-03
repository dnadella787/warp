#include "warp/codegen/stub_generator.hpp"

#include <stdexcept>

namespace warp::codegen {

generated_stub_set stub_generator::generate(const spec_ast &spec, const stub_generator_options &options) const {
	return generate(build_api_model(spec, options.namespace_name), options);
}

generated_stub_set stub_generator::generate(const api_model &model, const stub_generator_options &options) const {
	if (options.data_header_name.empty()) {
		throw std::invalid_argument("data_header_name cannot be empty");
	}
	if (options.resource_header_name.empty()) {
		throw std::invalid_argument("resource_header_name cannot be empty");
	}
	if (model.cpp_namespace.empty()) {
		throw std::invalid_argument("namespace_name cannot be empty");
	}
	if (!options.namespace_name.empty() && options.namespace_name != model.cpp_namespace) {
		throw std::invalid_argument(
		    "namespace_name must be empty or match model.cpp_namespace when generating from an api_model");
	}

	const data_class_emitter data_emitter;
	const resource_stub_emitter resource_emitter;

	generated_stub_set generated;
	generated.data_header.path = options.data_header_name;
	generated.data_header.content = data_emitter.emit_header(model);

	generated.resource_header.path = options.resource_header_name;
	generated.resource_header.content = resource_emitter.emit_header(
	    model, {.data_header_include = generated.data_header.path, .include_data_header = true});

	return generated;
}

generated_stub_set stub_generator::generate_from_yaml(std::string_view yaml_text,
                                                      const stub_generator_options &options) const {
	return generate(parse_spec_ast(yaml_text), options);
}

generated_stub_set stub_generator::generate_from_file(const std::filesystem::path &yaml_path,
                                                      const stub_generator_options &options) const {
	return generate(load_spec_ast(yaml_path), options);
}

} // namespace warp::codegen
