#include "data_class_emitter.hpp"

#include "codegen/model.hpp"

#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace warp::codegen {

namespace {

void append_line(std::string &output, const std::string &line = {}) {
	output.append(line);
	output.push_back('\n');
}

std::string escape_string_literal(const std::string &value) {
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
		case '\t':
			out.append("\\t");
			break;
		default:
			out.push_back(c);
			break;
		}
	}
	return out;
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

struct generated_field {
	std::string type_name;
	std::string member_name;
	std::optional<std::string> json_name;
};

std::vector<generated_field> schema_fields(const object_schema_model &schema) {
	std::vector<generated_field> fields;
	fields.reserve(schema.fields.size());
	for (const auto &field : schema.fields) {
		fields.push_back({.type_name = field_cpp_type(field.type, field.required),
		                  .member_name = field.member_name,
		                  .json_name = field.json_name});
	}
	return fields;
}

std::vector<generated_field> request_fields(const endpoint_model &endpoint) {
	std::vector<generated_field> fields;
	fields.reserve(endpoint.request.parameters.size() + (endpoint.request.body_type_name.has_value() ? 1U : 0U));
	for (const auto &parameter : endpoint.request.parameters) {
		fields.push_back(
		    {.type_name = field_cpp_type(parameter.type, parameter.required), .member_name = parameter.member_name});
	}
	if (endpoint.request.body_type_name.has_value()) {
		fields.push_back({.type_name = *endpoint.request.body_type_name, .member_name = "body"});
	}
	return fields;
}

std::vector<generated_field> response_fields(const endpoint_model &endpoint) {
	if (!endpoint.response.body_type_name.has_value()) {
		return {};
	}
	return {{.type_name = *endpoint.response.body_type_name, .member_name = "body"}};
}

void emit_data_class(std::string &output, const std::string &name, const std::vector<generated_field> &fields,
                     const std::vector<std::string> &extra_public_lines = {}) {
	append_line(output, "struct " + name + " {");
	for (const auto &field : fields) {
		append_line(output, "\t" + field.type_name + " " + field.member_name + " {};");
	}
	for (const auto &line : extra_public_lines) {
		append_line(output, "\t" + line);
	}
	append_line(output, "};");
	append_line(output);
}

template <typename T>
std::string scalar_literal(const T &value) {
	if constexpr (std::is_same_v<T, bool>) {
		return value ? "true" : "false";
	} else if constexpr (std::is_floating_point_v<T>) {
		return std::to_string(value);
	} else {
		return std::to_string(value);
	}
}

template <typename T>
std::optional<std::string> optional_literal(const std::optional<T> &value) {
	if (!value.has_value()) {
		return std::nullopt;
	}
	return scalar_literal(*value);
}

std::optional<std::string> optional_numeric_literal(const std::optional<numeric_validation_value> &value) {
	if (!value.has_value()) {
		return std::nullopt;
	}
	return std::visit([](const auto &numeric) { return scalar_literal(numeric); }, *value);
}

std::optional<std::string> numeric_min_literal(const validation_rules &validation) {
	return optional_numeric_literal(validation.min);
}

std::optional<std::string> numeric_max_literal(const validation_rules &validation) {
	return optional_numeric_literal(validation.max);
}

std::optional<std::string> string_min_length_literal(const validation_rules &validation) {
	return optional_literal(validation.min_length);
}

std::optional<std::string> string_max_length_literal(const validation_rules &validation) {
	return optional_literal(validation.max_length);
}

std::string validation_argument(const schema_type &type, const validation_rules &validation) {
	switch (type.type) {
	case schema_type::kind::string_value: {
		const auto min_length = string_min_length_literal(validation);
		const auto max_length = string_max_length_literal(validation);
		if (!min_length.has_value() && !max_length.has_value()) {
			return {};
		}

		std::string output = ", json_field_validation<std::string>{";
		bool needs_separator = false;
		if (min_length.has_value()) {
			output.append(".min_length = " + *min_length);
			needs_separator = true;
		}
		if (max_length.has_value()) {
			if (needs_separator) {
				output.append(", ");
			}
			output.append(".max_length = " + *max_length);
		}
		output.push_back('}');
		return output;
	}
	case schema_type::kind::int64_value: {
		const auto min = numeric_min_literal(validation);
		const auto max = numeric_max_literal(validation);
		if (!min.has_value() && !max.has_value()) {
			return {};
		}

		std::string output = ", json_field_validation<std::int64_t>{";
		bool needs_separator = false;
		if (min.has_value()) {
			output.append(".min = " + *min);
			needs_separator = true;
		}
		if (max.has_value()) {
			if (needs_separator) {
				output.append(", ");
			}
			output.append(".max = " + *max);
		}
		output.push_back('}');
		return output;
	}
	case schema_type::kind::double_value: {
		const auto min = numeric_min_literal(validation);
		const auto max = numeric_max_literal(validation);
		if (!min.has_value() && !max.has_value()) {
			return {};
		}

		std::string output = ", json_field_validation<double>{";
		bool needs_separator = false;
		if (min.has_value()) {
			output.append(".min = " + *min);
			needs_separator = true;
		}
		if (max.has_value()) {
			if (needs_separator) {
				output.append(", ");
			}
			output.append(".max = " + *max);
		}
		output.push_back('}');
		return output;
	}
	case schema_type::kind::bool_value:
	case schema_type::kind::object_value:
	case schema_type::kind::array_value:
		return {};
	}
	throw std::invalid_argument("unsupported schema type");
}

void emit_json_contract_specialization(std::string &output, std::string_view cpp_namespace,
                                       const object_schema_model &schema, const std::vector<generated_field> &fields) {
	const auto qualified_name = std::string(cpp_namespace) + "::" + schema.name;
	append_line(output, "template <>");
	append_line(output, "struct json_object_contract<" + qualified_name + "> {");
	append_line(output, "\tstatic constexpr std::string_view type_name = \"" + schema.name + "\";");
	append_line(output, "\tstatic constexpr auto fields = std::make_tuple(");
	for (std::size_t i = 0; i < schema.fields.size(); ++i) {
		const auto &field = schema.fields[i];
		const auto &generated = fields[i];
		const auto helper_name = field.required ? "make_required_json_field" : "make_optional_json_field";
		append_line(output, "\t\t" + std::string(helper_name) + "(\"" + escape_string_literal(field.json_name) + "\",");
		append_line(output, "\t\t\t&" + qualified_name + "::" + generated.member_name +
		                        validation_argument(field.type, field.validation) + ")" +
		                        (i + 1U == schema.fields.size() ? "" : ","));
	}
	append_line(output, "\t);");
	append_line(output, "};");
	append_line(output);
}

void emit_object_schema(std::string &output, std::string &contract_output, std::string_view cpp_namespace,
                        const object_schema_model &schema) {
	const auto fields = schema_fields(schema);
	emit_data_class(output, schema.name, fields);
	emit_json_contract_specialization(contract_output, cpp_namespace, schema, fields);
}

const object_schema_model *find_schema(const api_model &model, const std::string &name) {
	for (const auto &schema : model.schemas) {
		if (schema.name == name) {
			return &schema;
		}
	}
	return nullptr;
}

void emit_schema_with_dependencies(std::string &output, std::string &contract_output, const api_model &model,
                                   const object_schema_model &schema, std::set<std::string> &emitted) {
	if (emitted.contains(schema.name)) {
		return;
	}

	for (const auto &field : schema.fields) {
		if (field.type.type == schema_type::kind::object_value) {
			if (const auto *dependency = find_schema(model, field.type.object_name)) {
				emit_schema_with_dependencies(output, contract_output, model, *dependency, emitted);
			}
		} else if (field.type.type == schema_type::kind::array_value && field.type.element_type &&
		           field.type.element_type->type == schema_type::kind::object_value) {
			if (const auto *dependency = find_schema(model, field.type.element_type->object_name)) {
				emit_schema_with_dependencies(output, contract_output, model, *dependency, emitted);
			}
		}
	}

	emit_object_schema(output, contract_output, model.cpp_namespace, schema);
	emitted.insert(schema.name);
}

void emit_request_contract(std::string &output, const endpoint_model &endpoint) {
	emit_data_class(output, endpoint.request.name, request_fields(endpoint));
}

void emit_result_contract(std::string &output, const endpoint_model &endpoint) {
	emit_data_class(output, endpoint.result_name, response_fields(endpoint),
	                {"static constexpr unsigned status_code = " + std::to_string(endpoint.response.status_code) + ";"});
}

} // namespace

std::string data_class_emitter::emit_header(const api_model &model) const {
	if (model.cpp_namespace.empty()) {
		throw std::invalid_argument("namespace_name cannot be empty");
	}

	std::string output;
	output.reserve(8192);
	std::string contract_output;
	contract_output.reserve(4096);

	append_line(output, "#pragma once");
	append_line(output);
	append_line(output, "#include \"warp/codegen/json_object_contract.hpp\"");
	append_line(output);
	append_line(output, "#include <cstdint>");
	append_line(output, "#include <optional>");
	append_line(output, "#include <string>");
	append_line(output, "#include <utility>");
	append_line(output, "#include <vector>");
	append_line(output);
	append_line(output, "namespace " + model.cpp_namespace + " {");
	append_line(output);

	std::set<std::string> emitted_schemas;
	for (const auto &schema : model.schemas) {
		emit_schema_with_dependencies(output, contract_output, model, schema, emitted_schemas);
	}

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			emit_request_contract(output, endpoint);
			emit_result_contract(output, endpoint);
		}
	}

	append_line(output, "template <typename T>");
	append_line(output, "\trequires warp::codegen::json_contract_type<T>");
	append_line(output, "inline T tag_invoke(boost::json::value_to_tag<T>, const boost::json::value &value) {");
	append_line(output, "\treturn warp::codegen::parse_json_object<T>(value);");
	append_line(output, "}");
	append_line(output);
	append_line(output, "template <typename T>");
	append_line(output, "\trequires warp::codegen::json_contract_type<T>");
	append_line(output, "inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value, T &&input) {");
	append_line(output, "\twarp::codegen::serialize_json_object(value, std::forward<T>(input));");
	append_line(output, "}");
	append_line(output);
	append_line(output, "} // namespace " + model.cpp_namespace);
	append_line(output);
	append_line(output, "namespace warp::codegen {");
	append_line(output);
	output.append(contract_output);
	append_line(output, "} // namespace warp::codegen");
	return output;
}

} // namespace warp::codegen
