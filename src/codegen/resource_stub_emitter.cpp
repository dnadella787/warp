#include "warp/codegen/resource_stub_emitter.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace warp::codegen {

namespace {

void append_line(std::string &output, const std::string &line = {}) {
	output.append(line);
	output.push_back('\n');
}

std::string escape_cpp_string_literal_contents(std::string_view value) {
	std::string out;
	out.reserve(value.size());
	for (char c : value) {
		switch (c) {
		case '\\':
			out.append("\\\\");
			break;
		case '"':
			out.append("\\\"");
			break;
		case '\n':
			out.append("\\n");
			break;
		case '\r':
			out.append("\\r");
			break;
		case '\t':
			out.append("\\t");
			break;
		default: {
			const auto uc = static_cast<unsigned char>(c);
			if (uc < 0x20) {
				static constexpr char hex[] = "0123456789ABCDEF";
				out.append("\\x");
				out.push_back(hex[(uc >> 4) & 0xF]);
				out.push_back(hex[uc & 0xF]);
			} else {
				out.push_back(c);
			}
			break;
		}
		}
	}
	return out;
}

std::string cpp_string_literal(std::string_view value) {
	return "\"" + escape_cpp_string_literal_contents(value) + "\"";
}

std::string method_expression(http_method method) {
	switch (method) {
	case http_method::get:
		return "warp::method::get";
	case http_method::post:
		return "warp::method::post";
	case http_method::put:
		return "warp::method::put";
	case http_method::patch:
		return "warp::method::patch";
	case http_method::delete_:
		return "warp::method::delete_";
	}
	throw std::invalid_argument("unsupported HTTP method");
}

std::string cpp_type(const schema_type &type) {
	switch (type.type) {
	case schema_type::kind::string_value:
		return "std::string";
	case schema_type::kind::int64_value:
		return "std::int64_t";
	case schema_type::kind::double_value:
		return "double";
	case schema_type::kind::bool_value:
		return "bool";
	case schema_type::kind::object_value:
		if (type.object_name.empty()) {
			throw std::invalid_argument("object schema reference must include a type name");
		}
		return type.object_name;
	case schema_type::kind::array_value:
		if (!type.element_type) {
			throw std::invalid_argument("array schema must include an element type");
		}
		return "std::vector<" + cpp_type(*type.element_type) + ">";
	}
	throw std::invalid_argument("unsupported schema type");
}

std::string field_cpp_type(const schema_type &type, bool required) {
	const auto base = cpp_type(type);
	return required ? base : "std::optional<" + base + ">";
}

std::string request_type_name(const api_model &model, const endpoint_model &endpoint) {
	return model.cpp_namespace + "::" + endpoint.request.name;
}

std::string response_type_name(const api_model &model, const endpoint_model &endpoint) {
	return model.cpp_namespace + "::" + endpoint.result_name;
}

std::string route_alias_name(const endpoint_model &endpoint) {
	if (endpoint.query_route.has_value()) {
		return endpoint.query_route->spec_name;
	}
	return endpoint.request.name + "_route";
}

std::string request_contract_alias_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_contract";
}

std::string response_contract_alias_name(const endpoint_model &endpoint) {
	return endpoint.result_name + "_contract";
}

std::string generated_detail_namespace(const api_model &model) {
	return model.cpp_namespace + "::codegen_detail";
}

std::string qualified_generated_detail_name(const api_model &model, const std::string &name) {
	return generated_detail_namespace(model) + "::" + name;
}

std::string endpoint_binding_alias_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_endpoint";
}

std::string handler_selector_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_handler_selector";
}

std::string accessor_name(const std::string &type_name, const std::string &field_name) {
	return type_name + "_" + field_name + "_accessor";
}

[[nodiscard]] bool contains_parameter(const std::vector<std::string> &parameters, std::string_view name) {
	return std::find(parameters.begin(), parameters.end(), name) != parameters.end();
}

std::string query_constraint_expression(const query_route_model &query_route, const route_group_model &group,
                                        std::string_view parameter_name) {
	if (contains_parameter(query_route.required_parameters, parameter_name)) {
		return "warp::http::required_query<" + cpp_string_literal(parameter_name) + ">";
	}
	if (contains_parameter(query_route.accepted_parameters, parameter_name)) {
		return "warp::http::optional_query<" + cpp_string_literal(parameter_name) + ">";
	}
	return "warp::http::forbidden_query<" + cpp_string_literal(parameter_name) + ">";
}

void emit_route_alias(std::string &output, const endpoint_model &endpoint) {
	std::string alias = "using " + route_alias_name(endpoint) + " = warp::http::route_spec<" +
	                    method_expression(endpoint.method) + ", " + cpp_string_literal(endpoint.path);
	alias.append(">;");
	append_line(output, alias);
}

void emit_query_route_spec_aliases(std::string &output, const resource_model &resource) {
	for (const auto &endpoint : resource.endpoints) {
		if (endpoint.query_route.has_value()) {
			continue;
		}
		emit_route_alias(output, endpoint);
	}

	for (const auto &group : resource.route_groups) {
		for (const auto endpoint_index : group.query_route_endpoint_indices) {
			const auto &endpoint = resource.endpoints[endpoint_index];
			std::string alias = "using " + route_alias_name(endpoint) + " = warp::http::route_spec<" +
			                    method_expression(endpoint.method) + ", " + cpp_string_literal(endpoint.path);
			for (const auto &parameter : group.routing_query_parameters) {
				alias.append(", ");
				alias.append(query_constraint_expression(*endpoint.query_route, group, parameter));
			}
			alias.append(">;");
			append_line(output, alias);
		}

		if (group.query_route_endpoint_indices.size() < 2) {
			continue;
		}

		std::string assertion = "static_assert(warp::http::deterministic_route_definitions<";
		for (std::size_t i = 0; i < group.query_route_endpoint_indices.size(); ++i) {
			if (i > 0) {
				assertion.append(", ");
			}
			assertion.append(route_alias_name(resource.endpoints[group.query_route_endpoint_indices[i]]));
		}
		assertion.append(">(), \"generated query routes must resolve deterministically\");");
		append_line(output, assertion);
	}
}

std::string binding_expression(const std::string &request_type, const parameter_model &parameter) {
	const auto accessor = accessor_name(request_type, parameter.member_name);
	const auto source_name = cpp_string_literal(parameter.source_name);
	switch (parameter.location) {
	case parameter_location::path:
		return "warp::codegen::path_binding<" + accessor + ", " + source_name + ">";
	case parameter_location::query:
		return "warp::codegen::query_binding<" + accessor + ", " + source_name + ">";
	case parameter_location::header:
		return "warp::codegen::header_binding<" + accessor + ", " + source_name + ">";
	}
	throw std::invalid_argument("unsupported parameter location");
}

void emit_request_field_accessor(std::string &output, const std::string &request_type, const std::string &value_type,
                                 const std::string &field_name) {
	append_line(output, "struct " + accessor_name(request_type, field_name) + " {");
	append_line(output, "    using class_type = " + request_type + ";");
	append_line(output, "    using value_type = " + value_type + ";");
	append_line(output, "    static void set(class_type &value, value_type member_value) {");
	append_line(output, "        value.set_" + field_name + "(std::move(member_value));");
	append_line(output, "    }");
	append_line(output, "};");
}

void emit_response_field_accessor(std::string &output, const std::string &response_type, const std::string &value_type,
                                  const std::string &field_name) {
	append_line(output, "struct " + accessor_name(response_type, field_name) + " {");
	append_line(output, "    using class_type = " + response_type + ";");
	append_line(output, "    using value_type = " + value_type + ";");
	append_line(output, "    [[nodiscard]] static const value_type &get(const class_type &value) noexcept {");
	append_line(output, "        return value." + field_name + "();");
	append_line(output, "    }");
	append_line(output, "    [[nodiscard]] static value_type &&get(class_type &&value) noexcept {");
	append_line(output, "        return std::move(value)." + field_name + "();");
	append_line(output, "    }");
	append_line(output, "};");
}

void emit_endpoint_accessors(std::string &output, const endpoint_model &endpoint) {
	for (const auto &parameter : endpoint.request.parameters) {
		emit_request_field_accessor(output, endpoint.request.name, field_cpp_type(parameter.type, parameter.required),
		                            parameter.member_name);
		append_line(output);
	}
	if (endpoint.request.body_type_name.has_value()) {
		emit_request_field_accessor(output, endpoint.request.name, *endpoint.request.body_type_name, "body");
		append_line(output);
	}
	if (endpoint.response.body_type_name.has_value()) {
		emit_response_field_accessor(output, endpoint.result_name, *endpoint.response.body_type_name, "body");
		append_line(output);
	}
}

void emit_request_contract_traits(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto request_type = request_type_name(model, endpoint);
	const auto contract_alias = qualified_generated_detail_name(model, request_contract_alias_name(endpoint));
	append_line(output, "template <>");
	append_line(output, "struct request_contract_traits<" + request_type + "> : " + contract_alias + " {};");
	append_line(output);
}

void emit_response_contract_traits(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto response_type = response_type_name(model, endpoint);
	const auto contract_alias = qualified_generated_detail_name(model, response_contract_alias_name(endpoint));
	append_line(output, "template <>");
	append_line(output, "struct response_contract_traits<" + response_type + "> : " + contract_alias + " {};");
	append_line(output);
}

void emit_request_contract_alias(std::string &output, const endpoint_model &endpoint) {
	append_line(output, "using " + request_contract_alias_name(endpoint) +
	                        " = warp::codegen::generated_request_contract<" + endpoint.request.name);
	for (const auto &parameter : endpoint.request.parameters) {
		append_line(output, "    , " + binding_expression(endpoint.request.name, parameter));
	}
	if (endpoint.request.body_type_name.has_value()) {
		append_line(output,
		            "    , warp::codegen::json_body_binding<" + accessor_name(endpoint.request.name, "body") + ">");
	}
	append_line(output, ">;");
}

void emit_response_contract_alias(std::string &output, const endpoint_model &endpoint) {
	if (endpoint.response.body_type_name.has_value()) {
		append_line(output, "using " + response_contract_alias_name(endpoint) +
		                        " = warp::codegen::body_response_contract<" + endpoint.result_name + ", " +
		                        accessor_name(endpoint.result_name, "body") + ">;");
	} else {
		append_line(output, "using " + response_contract_alias_name(endpoint) +
		                        " = warp::codegen::empty_response_contract<" + endpoint.result_name + ">;");
	}
}

void emit_handler_selector(std::string &output, const endpoint_model &endpoint) {
	append_line(output, "struct " + handler_selector_name(endpoint) + " {");
	append_line(output, "    template <typename Signature, typename Service>");
	append_line(output, "    static consteval bool matches() {");
	append_line(output,
	            "        return requires { static_cast<Signature>(&Service::" + endpoint.handler_name + "); };");
	append_line(output, "    }");
	append_line(output, "    template <typename Signature, typename Service>");
	append_line(output, "    static constexpr Signature get() {");
	append_line(output, "        return static_cast<Signature>(&Service::" + endpoint.handler_name + ");");
	append_line(output, "    }");
	append_line(output, "};");
}

void emit_endpoint_binding_alias(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto endpoint_alias = endpoint_binding_alias_name(endpoint);
	const auto request_type = request_type_name(model, endpoint);
	const auto response_type = response_type_name(model, endpoint);
	const auto selector_name = qualified_generated_detail_name(model, handler_selector_name(endpoint));
	append_line(output, "template <typename Service>");
	append_line(output, "using " + endpoint_alias + " = warp::codegen::endpoint_binding<");
	append_line(output, "    Service,");
	append_line(output, "    " + route_alias_name(endpoint) + ",");
	append_line(output, "    warp::codegen::request_contract_traits<" + request_type + ">,");
	append_line(output, "    " + response_type + ",");
	append_line(output, "    [](Service &service, " + request_type + " &&typed_request) -> decltype(auto) {");
	append_line(output, "        return warp::codegen::invoke_endpoint_handler_overload<");
	append_line(output, "            " + response_type + ",");
	append_line(output, "            " + request_type + ",");
	append_line(output, "            Service,");
	append_line(output, "            " + selector_name + ">(service, std::move(typed_request));");
	append_line(output, "    }>;");
	append_line(output);
}

void emit_resource_routes(std::string &output, const api_model &model, const resource_model &resource) {
	emit_query_route_spec_aliases(output, resource);
	if (!resource.endpoints.empty()) {
		append_line(output);
	}

	for (const auto &endpoint : resource.endpoints) {
		emit_endpoint_binding_alias(output, model, endpoint);
	}

	append_line(output, "template <typename Service>");
	append_line(output, "using " + resource.routes_class_name + " = warp::codegen::generated_resource<");
	append_line(output, "    Service");
	for (const auto &endpoint : resource.endpoints) {
		append_line(output, "    , " + endpoint_binding_alias_name(endpoint) + "<Service>");
	}
	append_line(output, ">;");
	append_line(output);
}

} // namespace

std::string resource_stub_emitter::emit_header(const api_model &model,
                                               const resource_stub_emitter_options &options) const {
	if (model.cpp_namespace.empty()) {
		throw std::invalid_argument("namespace_name cannot be empty");
	}
	if (options.include_data_header && options.data_header_include.empty()) {
		throw std::invalid_argument("data_header_include cannot be empty");
	}

	std::string output;
	output.reserve(8192);

	append_line(output, "#pragma once");
	append_line(output);
	if (options.include_data_header) {
		append_line(output, "#include \"" + options.data_header_include + "\"");
	}
	append_line(output, "#include \"warp/codegen/http_adapter.hpp\"");
	append_line(output);
	append_line(output, "namespace " + generated_detail_namespace(model) + " {");
	append_line(output);

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			emit_endpoint_accessors(output, endpoint);
			emit_request_contract_alias(output, endpoint);
			emit_response_contract_alias(output, endpoint);
			emit_handler_selector(output, endpoint);
			append_line(output);
		}
	}

	append_line(output, "} // namespace " + generated_detail_namespace(model));
	append_line(output);
	append_line(output, "namespace warp::codegen {");
	append_line(output);

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			emit_request_contract_traits(output, model, endpoint);
			emit_response_contract_traits(output, model, endpoint);
		}
	}

	append_line(output, "} // namespace warp::codegen");
	append_line(output);
	append_line(output, "namespace " + model.cpp_namespace + " {");
	append_line(output);

	for (const auto &resource : model.resources) {
		emit_resource_routes(output, model, resource);
	}

	append_line(output, "} // namespace " + model.cpp_namespace);
	return output;
}

} // namespace warp::codegen
