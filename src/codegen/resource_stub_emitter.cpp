#include "resource_stub_emitter.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>

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

std::string generated_detail_namespace(const api_model &model) {
	return model.cpp_namespace + "::codegen_detail";
}

std::string qualified_generated_detail_name(const api_model &model, const std::string &name) {
	return generated_detail_namespace(model) + "::" + name;
}

std::string endpoint_binding_alias_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_endpoint";
}

std::string handler_result_alias_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_handler_result";
}

std::string handler_selector_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_handler_selector";
}

std::string request_validator_name(const endpoint_model &endpoint) {
	return endpoint.request.name + "_validator";
}

std::string schema_validator_name(std::string_view schema_name) {
	return std::string(schema_name) + "_validator";
}

template <typename T>
std::string numeric_literal(T value) {
	return std::to_string(value);
}

template <typename Fn>
void visit_numeric_validation_value(const numeric_validation_value &value, Fn &&fn) {
	std::visit([&](const auto &numeric) { fn(numeric); }, value);
}

template <typename Fn>
void with_numeric_min(const validation_rules &validation, Fn &&fn) {
	if (validation.min.has_value()) {
		visit_numeric_validation_value(*validation.min, std::forward<Fn>(fn));
	}
}

template <typename Fn>
void with_numeric_max(const validation_rules &validation, Fn &&fn) {
	if (validation.max.has_value()) {
		visit_numeric_validation_value(*validation.max, std::forward<Fn>(fn));
	}
}

template <typename Fn>
void with_min_length(const validation_rules &validation, Fn &&fn) {
	if (validation.min_length.has_value()) {
		fn(*validation.min_length);
	}
}

template <typename Fn>
void with_max_length(const validation_rules &validation, Fn &&fn) {
	if (validation.max_length.has_value()) {
		fn(*validation.max_length);
	}
}

std::string_view parameter_wire_name(const parameter_model &parameter) {
	return parameter.source_name;
}

std::string_view field_wire_name(const field_model &field) {
	return field.json_name;
}

const object_schema_model *find_schema(const api_model &model, std::string_view name) {
	for (const auto &schema : model.schemas) {
		if (schema.name == name) {
			return &schema;
		}
	}
	return nullptr;
}

bool schema_type_has_validation(const api_model &model, const schema_type &type);

bool schema_has_validation(const api_model &model, const object_schema_model &schema) {
	for (const auto &field : schema.fields) {
		if (!field.validation.empty() || schema_type_has_validation(model, field.type)) {
			return true;
		}
	}
	return false;
}

bool schema_type_has_validation(const api_model &model, const schema_type &type) {
	switch (type.type) {
	case schema_type::kind::object_value: {
		const auto *schema = find_schema(model, type.object_name);
		return schema != nullptr && schema_has_validation(model, *schema);
	}
	case schema_type::kind::array_value:
		return type.element_type != nullptr && schema_type_has_validation(model, *type.element_type);
	default:
		return false;
	}
}

[[nodiscard]] std::optional<warp::http::compiled_query_constraint>
find_query_constraint(const query_route_model &query_route, std::string_view name) {
	for (const auto constraint : query_route.constraints) {
		if (constraint.name == name) {
			return constraint;
		}
	}
	return std::nullopt;
}

std::string query_constraint_expression(const query_route_model &query_route, std::string_view parameter_name) {
	const auto constraint = find_query_constraint(query_route, parameter_name);
	if (!constraint.has_value()) {
		return "warp::http::forbidden_query<" + cpp_string_literal(parameter_name) + ">";
	}
	if (constraint->value.has_value()) {
		if (constraint->presence == warp::http::query_constraint_presence::required) {
			return "warp::http::required_query_value<" + cpp_string_literal(parameter_name) + ", " +
			       cpp_string_literal(*constraint->value) + ">";
		}
		return "warp::http::optional_query_value<" + cpp_string_literal(parameter_name) + ", " +
		       cpp_string_literal(*constraint->value) + ">";
	}
	if (constraint->presence == warp::http::query_constraint_presence::required) {
		return "warp::http::required_query<" + cpp_string_literal(parameter_name) + ">";
	}
	if (constraint->presence == warp::http::query_constraint_presence::optional) {
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
				alias.append(query_constraint_expression(*endpoint.query_route, parameter));
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
	const auto source_name = cpp_string_literal(parameter_wire_name(parameter));
	switch (parameter.location) {
	case parameter_location::path:
		return "warp::codegen::path_field_binding<&" + request_type + "::" + parameter.member_name + ", " +
		       source_name + ">";
	case parameter_location::query:
		return "warp::codegen::query_field_binding<&" + request_type + "::" + parameter.member_name + ", " +
		       source_name + ">";
	case parameter_location::header:
		return "warp::codegen::header_field_binding<&" + request_type + "::" + parameter.member_name + ", " +
		       source_name + ">";
	}
	throw std::invalid_argument("unsupported parameter location");
}

void emit_string_validation_lines(std::string &output, std::string_view indent, std::string_view location_literal,
                                  std::string_view wire_name_literal, std::string_view value_expression,
                                  const validation_rules &validation, std::string_view path_prefix_expression = {}) {
	with_min_length(validation, [&](const auto min_length) {
		std::string line(indent);
		line.append("if (auto error = warp::codegen::validate_min_length(");
		line.append(location_literal);
		line.append(", ");
		line.append(wire_name_literal);
		line.append(", ");
		line.append(value_expression);
		line.append(", ");
		line.append(numeric_literal(min_length));
		if (!path_prefix_expression.empty()) {
			line.append(", ");
			line.append(path_prefix_expression);
		}
		line.append("); error.has_value()) {");
		append_line(output, line);
		append_line(output, std::string(indent) + "    return error;");
		append_line(output, std::string(indent) + "}");
	});

	with_max_length(validation, [&](const auto max_length) {
		std::string line(indent);
		line.append("if (auto error = warp::codegen::validate_max_length(");
		line.append(location_literal);
		line.append(", ");
		line.append(wire_name_literal);
		line.append(", ");
		line.append(value_expression);
		line.append(", ");
		line.append(numeric_literal(max_length));
		if (!path_prefix_expression.empty()) {
			line.append(", ");
			line.append(path_prefix_expression);
		}
		line.append("); error.has_value()) {");
		append_line(output, line);
		append_line(output, std::string(indent) + "    return error;");
		append_line(output, std::string(indent) + "}");
	});
}

void emit_numeric_validation_lines(std::string &output, std::string_view indent, std::string_view location_literal,
                                   std::string_view wire_name_literal, std::string_view value_expression,
                                   const validation_rules &validation, std::string_view path_prefix_expression = {}) {
	with_numeric_min(validation, [&](const auto minimum) {
		std::string line(indent);
		line.append("if (auto error = warp::codegen::validate_min_value(");
		line.append(location_literal);
		line.append(", ");
		line.append(wire_name_literal);
		line.append(", ");
		line.append(value_expression);
		line.append(", ");
		line.append(numeric_literal(minimum));
		if (!path_prefix_expression.empty()) {
			line.append(", ");
			line.append(path_prefix_expression);
		}
		line.append("); error.has_value()) {");
		append_line(output, line);
		append_line(output, std::string(indent) + "    return error;");
		append_line(output, std::string(indent) + "}");
	});

	with_numeric_max(validation, [&](const auto maximum) {
		std::string line(indent);
		line.append("if (auto error = warp::codegen::validate_max_value(");
		line.append(location_literal);
		line.append(", ");
		line.append(wire_name_literal);
		line.append(", ");
		line.append(value_expression);
		line.append(", ");
		line.append(numeric_literal(maximum));
		if (!path_prefix_expression.empty()) {
			line.append(", ");
			line.append(path_prefix_expression);
		}
		line.append("); error.has_value()) {");
		append_line(output, line);
		append_line(output, std::string(indent) + "    return error;");
		append_line(output, std::string(indent) + "}");
	});
}

void emit_value_validation_lines(std::string &output, std::string_view indent, std::string_view location_literal,
                                 std::string_view wire_name_literal, std::string_view value_expression,
                                 const schema_type &type, const validation_rules &validation,
                                 std::string_view path_prefix_expression) {
	switch (type.type) {
	case schema_type::kind::string_value:
		emit_string_validation_lines(output, indent, location_literal, wire_name_literal, value_expression, validation,
		                             path_prefix_expression);
		break;
	case schema_type::kind::int64_value:
	case schema_type::kind::double_value:
		emit_numeric_validation_lines(output, indent, location_literal, wire_name_literal, value_expression, validation,
		                              path_prefix_expression);
		break;
	default:
		break;
	}
}

void emit_request_parameter_validation(std::string &output, const parameter_model &parameter) {
	if (parameter.validation.empty()) {
		return;
	}

	const auto location_literal = parameter.location == parameter_location::path    ? "\"path parameter\""
	                              : parameter.location == parameter_location::query ? "\"query parameter\""
	                                                                                : "\"header\"";
	const auto wire_name_literal = cpp_string_literal(parameter_wire_name(parameter));
	const auto member_access = "value." + parameter.member_name;

	if (!parameter.required) {
		append_line(output, "        if (" + member_access + ".has_value()) {");
		emit_value_validation_lines(output, "            ", location_literal, wire_name_literal, "*" + member_access,
		                            parameter.type, parameter.validation, {});
		append_line(output, "        }");
		return;
	}

	emit_value_validation_lines(output, "        ", location_literal, wire_name_literal, member_access, parameter.type,
	                            parameter.validation, {});
}

void emit_schema_validator(std::string &output, const api_model &model, const object_schema_model &schema,
                           std::unordered_set<std::string> &emitted);

void emit_nested_schema_validation(std::string &output, const api_model &model, const field_model &field,
                                   std::string_view value_expression, std::string_view indent,
                                   std::string_view path_prefix_expression) {
	if (field.type.type == schema_type::kind::object_value) {
		const auto *child_schema = find_schema(model, field.type.object_name);
		if (child_schema == nullptr || !schema_has_validation(model, *child_schema)) {
			return;
		}

		const auto child_path = "warp::codegen::append_validation_path(" + std::string(path_prefix_expression) + ", " +
		                        cpp_string_literal(field_wire_name(field)) + ")";
		append_line(output, std::string(indent) + "if (auto error = " + schema_validator_name(child_schema->name) +
		                        "::validate(" + std::string(value_expression) + ", " + child_path +
		                        "); error.has_value()) {");
		append_line(output, std::string(indent) + "    return error;");
		append_line(output, std::string(indent) + "}");
		return;
	}

	if (field.type.type != schema_type::kind::array_value || field.type.element_type == nullptr ||
	    field.type.element_type->type != schema_type::kind::object_value) {
		return;
	}

	const auto *child_schema = find_schema(model, field.type.element_type->object_name);
	if (child_schema == nullptr || !schema_has_validation(model, *child_schema)) {
		return;
	}

	append_line(output, std::string(indent) + "const auto field_path = warp::codegen::append_validation_path(" +
	                        std::string(path_prefix_expression) + ", " + cpp_string_literal(field_wire_name(field)) +
	                        ");");
	append_line(output, std::string(indent) + "for (std::size_t i = 0; i < " + std::string(value_expression) +
	                        ".size(); ++i) {");
	append_line(output, std::string(indent) + "    if (auto error = " + schema_validator_name(child_schema->name) +
	                        "::validate(" + std::string(value_expression) +
	                        "[i], "
	                        "warp::codegen::append_validation_index(field_path, i)); error.has_value()) {");
	append_line(output, std::string(indent) + "        return error;");
	append_line(output, std::string(indent) + "    }");
	append_line(output, std::string(indent) + "}");
}

void emit_schema_field_validation(std::string &output, const api_model &model, const field_model &field) {
	if (field.validation.empty() && !schema_type_has_validation(model, field.type)) {
		return;
	}

	const auto wire_name_literal = cpp_string_literal(field_wire_name(field));
	const auto member_access = "value." + field.member_name;

	if (!field.required) {
		append_line(output, "        if (" + member_access + ".has_value()) {");
		emit_value_validation_lines(output, "            ", "\"JSON body field\"", wire_name_literal,
		                            "*" + member_access, field.type, field.validation, "field_path_prefix");
		emit_nested_schema_validation(output, model, field, "*" + member_access, "            ", "field_path_prefix");
		append_line(output, "        }");
		return;
	}

	emit_value_validation_lines(output, "        ", "\"JSON body field\"", wire_name_literal, member_access, field.type,
	                            field.validation, "field_path_prefix");
	emit_nested_schema_validation(output, model, field, member_access, "        ", "field_path_prefix");
}

void emit_schema_validator(std::string &output, const api_model &model, const object_schema_model &schema,
                           std::unordered_set<std::string> &emitted) {
	if (!emitted.insert(schema.name).second) {
		return;
	}

	for (const auto &field : schema.fields) {
		if (field.type.type == schema_type::kind::object_value) {
			if (const auto *child_schema = find_schema(model, field.type.object_name);
			    child_schema != nullptr && schema_has_validation(model, *child_schema)) {
				emit_schema_validator(output, model, *child_schema, emitted);
			}
		} else if (field.type.type == schema_type::kind::array_value && field.type.element_type != nullptr &&
		           field.type.element_type->type == schema_type::kind::object_value) {
			if (const auto *child_schema = find_schema(model, field.type.element_type->object_name);
			    child_schema != nullptr && schema_has_validation(model, *child_schema)) {
				emit_schema_validator(output, model, *child_schema, emitted);
			}
		}
	}

	append_line(output, "struct " + schema_validator_name(schema.name) + " {");
	append_line(output, "    static std::optional<warp::codegen::binding_error> validate(const " + model.cpp_namespace +
	                        "::" + schema.name + " &value, std::string_view field_path_prefix = {}) {");
	for (const auto &field : schema.fields) {
		emit_schema_field_validation(output, model, field);
	}
	append_line(output, "        return std::nullopt;");
	append_line(output, "    }");
	append_line(output, "};");
	append_line(output);
}

bool endpoint_has_request_validation(const api_model &model, const endpoint_model &endpoint) {
	for (const auto &parameter : endpoint.request.parameters) {
		if (!parameter.validation.empty()) {
			return true;
		}
	}

	if (!endpoint.request.body_type_name.has_value()) {
		return false;
	}

	const auto *body_schema = find_schema(model, *endpoint.request.body_type_name);
	return body_schema != nullptr && schema_has_validation(model, *body_schema);
}

void emit_request_validator(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto request_type = request_type_name(model, endpoint);
	append_line(output, "struct " + request_validator_name(endpoint) + " {");
	append_line(output, "    using request_type = " + request_type + ";");
	append_line(output);
	append_line(output, "    static std::optional<warp::codegen::binding_error> validate(const request_type &value) {");
	for (const auto &parameter : endpoint.request.parameters) {
		emit_request_parameter_validation(output, parameter);
	}
	if (endpoint.request.body_type_name.has_value()) {
		const auto *body_schema = find_schema(model, *endpoint.request.body_type_name);
		if (body_schema != nullptr && schema_has_validation(model, *body_schema)) {
			append_line(output, "        if (auto error = " + schema_validator_name(body_schema->name) +
			                        "::validate(value.body); error.has_value()) {");
			append_line(output, "            return error;");
			append_line(output, "        }");
		}
	}
	append_line(output, "        return std::nullopt;");
	append_line(output, "    }");
	append_line(output, "};");
	append_line(output);
}

void emit_request_contract_traits(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto request_type = request_type_name(model, endpoint);
	const auto has_validation = endpoint_has_request_validation(model, endpoint);
	append_line(output, "template <>");
	if (has_validation) {
		append_line(output, "struct request_contract_traits<" + request_type +
		                        "> : warp::codegen::validated_request_contract<");
		append_line(output, "    warp::codegen::generated_request_contract<");
		append_line(output, "        " + request_type);
	} else {
		append_line(output, "struct request_contract_traits<" + request_type +
		                        "> : warp::codegen::generated_request_contract<");
		append_line(output, "    " + request_type);
	}
	for (const auto &parameter : endpoint.request.parameters) {
		append_line(output, "        , " + binding_expression(request_type, parameter));
	}
	if (endpoint.request.body_type_name.has_value()) {
		append_line(output, "        , warp::codegen::json_body_field_binding<&" + request_type + "::body>");
	}
	if (has_validation) {
		append_line(output, "    >,");
		append_line(output,
		            "    " + qualified_generated_detail_name(model, request_validator_name(endpoint)) + "> {};");
	} else {
		append_line(output, "> {};");
	}
	append_line(output);
}

void emit_response_contract_traits(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto response_type = response_type_name(model, endpoint);
	if (!endpoint.response.body_type_name.has_value()) {
		append_line(output, "template <>");
		append_line(output, "struct response_contract_traits<" + response_type +
		                        "> : warp::codegen::empty_response_contract<" + response_type + "> {};");
		append_line(output);
		return;
	}

	append_line(output, "template <>");
	append_line(output, "struct response_contract_traits<" + response_type + "> {");
	append_line(output, "    using response_type = " + response_type + ";");
	append_line(output, "    static constexpr unsigned status_code = response_type::status_code;");
	append_line(output, "    static constexpr bool has_body = true;");
	append_line(output);
	append_line(output, "    static decltype(auto) body(const response_type &value) {");
	append_line(output, "        return (value.body);");
	append_line(output, "    }");
	append_line(output);
	append_line(output, "    static decltype(auto) body(response_type &&value) {");
	append_line(output, "        return (std::move(value).body);");
	append_line(output, "    }");
	append_line(output, "};");
	append_line(output);
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
	append_line(output, "using " + endpoint_alias + " = warp::codegen::generated_endpoint_binding<");
	append_line(output, "    Service,");
	append_line(output, "    " + route_alias_name(endpoint) + ",");
	append_line(output, "    warp::codegen::request_contract_traits<" + request_type + ">,");
	append_line(output, "    warp::codegen::response_contract_traits<" + response_type + ">,");
	append_line(output, "    " + selector_name + ">;");
	append_line(output);
}

void emit_resource_routes(std::string &output, const api_model &model, const resource_model &resource) {
	emit_query_route_spec_aliases(output, resource);
	if (!resource.endpoints.empty()) {
		append_line(output);
	}

	for (const auto &endpoint : resource.endpoints) {
		append_line(output, "using " + handler_result_alias_name(endpoint) + " = warp::codegen::handler_result<" +
		                        response_type_name(model, endpoint) + ">;");
	}
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

	std::unordered_set<std::string> emitted_schema_validators;
	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			if (endpoint.request.body_type_name.has_value()) {
				if (const auto *body_schema = find_schema(model, *endpoint.request.body_type_name);
				    body_schema != nullptr && schema_has_validation(model, *body_schema)) {
					emit_schema_validator(output, model, *body_schema, emitted_schema_validators);
				}
			}
		}
	}

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			if (endpoint_has_request_validation(model, endpoint)) {
				emit_request_validator(output, model, endpoint);
			}
		}
	}

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
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
