#include "warp/codegen/stub_generator.hpp"

#include <stdexcept>

namespace warp::codegen {

generated_stub_set stub_generator::generate(const api_spec &spec, const stub_generator_options &options) const {
	if (options.data_header_name.empty()) {
		throw std::invalid_argument("data_header_name cannot be empty");
	}
	if (options.resource_header_name.empty()) {
		throw std::invalid_argument("resource_header_name cannot be empty");
	}

	const std::string namespace_name = options.namespace_name.empty() ? spec.cpp_namespace : options.namespace_name;
	if (namespace_name.empty()) {
		throw std::invalid_argument("namespace_name cannot be empty");
	}

	const data_class_emitter data_emitter;
	const resource_stub_emitter resource_emitter;

	generated_stub_set generated;
	generated.data_header.path = options.data_header_name;
	generated.data_header.content = data_emitter.emit_header(spec, {.namespace_name = namespace_name});

	generated.resource_header.path = options.resource_header_name;
	generated.resource_header.content =
	    resource_emitter.emit_header(spec, {.namespace_name = namespace_name,
	                                        .data_header_include = generated.data_header.path,
	                                        .include_data_header = true});

	return generated;
}

generated_stub_set stub_generator::generate_from_yaml(std::string_view yaml_text,
                                                      const stub_generator_options &options) const {
	return generate(parse_api_spec(yaml_text), options);
}

generated_stub_set stub_generator::generate_from_file(const std::filesystem::path &yaml_path,
                                                      const stub_generator_options &options) const {
	return generate(load_api_spec(yaml_path), options);
}

} // namespace warp::codegen
