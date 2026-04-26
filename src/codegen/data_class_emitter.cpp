#include "data_class_emitter.hpp"

#include "codegen/model.hpp"

#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <utility>
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
	std::string accessor_name;
	std::string storage_name;
	std::optional<std::string> json_name;
};

std::vector<generated_field> schema_fields(const object_schema_model &schema) {
	std::vector<generated_field> fields;
	fields.reserve(schema.fields.size());
	for (const auto &field : schema.fields) {
		fields.push_back({.type_name = field_cpp_type(field.type, field.required),
		                  .accessor_name = field.member_name,
		                  .storage_name = field.member_name + "_",
		                  .json_name = field.json_name});
	}
	return fields;
}

std::vector<generated_field> request_fields(const endpoint_model &endpoint) {
	std::vector<generated_field> fields;
	fields.reserve(endpoint.request.parameters.size() + (endpoint.request.body_type_name.has_value() ? 1U : 0U));
	for (const auto &parameter : endpoint.request.parameters) {
		fields.push_back({.type_name = field_cpp_type(parameter.type, parameter.required),
		                  .accessor_name = parameter.member_name,
		                  .storage_name = parameter.member_name + "_"});
	}
	if (endpoint.request.body_type_name.has_value()) {
		fields.push_back(
		    {.type_name = *endpoint.request.body_type_name, .accessor_name = "body", .storage_name = "body_"});
	}
	return fields;
}

std::vector<generated_field> response_fields(const endpoint_model &endpoint) {
	if (!endpoint.response.body_type_name.has_value()) {
		return {};
	}
	return {{.type_name = *endpoint.response.body_type_name, .accessor_name = "body", .storage_name = "body_"}};
}

void emit_data_class(std::string &output, const std::string &name, const std::vector<generated_field> &fields,
                     const std::vector<std::string> &extra_public_lines = {}) {
	append_line(output, "class " + name + " {");
	append_line(output, "public:");
	append_line(output, "\tclass Builder {");
	append_line(output, "\tpublic:");
	for (const auto &field : fields) {
		append_line(output, "\t\tBuilder &" + field.accessor_name + "(" + field.type_name + " value) {");
		append_line(output, "\t\t\t" + field.storage_name + " = std::move(value);");
		append_line(output, "\t\t\treturn *this;");
		append_line(output, "\t\t}");
	}
	append_line(output);
	append_line(output, "\t\t[[nodiscard]] " + name + " build() && {");
	append_line(output, "\t\t\t" + name + " out;");
	for (const auto &field : fields) {
		append_line(output, "\t\t\tout." + field.storage_name + " = std::move(" + field.storage_name + ");");
	}
	append_line(output, "\t\t\treturn out;");
	append_line(output, "\t\t}");
	append_line(output);
	append_line(output, "\t\t[[nodiscard]] " + name + " build() const & {");
	append_line(output, "\t\t\t" + name + " out;");
	for (const auto &field : fields) {
		append_line(output, "\t\t\tout." + field.storage_name + " = " + field.storage_name + ";");
	}
	append_line(output, "\t\t\treturn out;");
	append_line(output, "\t\t}");
	append_line(output);
	append_line(output, "\tprivate:");
	for (const auto &field : fields) {
		append_line(output, "\t\t" + field.type_name + " " + field.storage_name + " {};");
	}
	append_line(output, "\t};");
	append_line(output);
	append_line(output, "\t" + name + "() = default;");
	for (const auto &line : extra_public_lines) {
		append_line(output, "\t" + line);
	}
	append_line(output, "\t[[nodiscard]] static Builder builder() {");
	append_line(output, "\t\treturn Builder {};");
	append_line(output, "\t}");
	for (const auto &field : fields) {
		append_line(output);
		append_line(output,
		            "\t[[nodiscard]] const " + field.type_name + " &" + field.accessor_name + "() const & noexcept {");
		append_line(output, "\t\treturn " + field.storage_name + ";");
		append_line(output, "\t}");
		append_line(output);
		append_line(output, "\t[[nodiscard]] " + field.type_name + " &" + field.accessor_name + "() & noexcept {");
		append_line(output, "\t\treturn " + field.storage_name + ";");
		append_line(output, "\t}");
		append_line(output);
		append_line(output, "\t[[nodiscard]] " + field.type_name + " &&" + field.accessor_name + "() && noexcept {");
		append_line(output, "\t\treturn std::move(" + field.storage_name + ");");
		append_line(output, "\t}");
		append_line(output);
		append_line(output, "\t" + name + " &set_" + field.accessor_name + "(" + field.type_name + " value) {");
		append_line(output, "\t\t" + field.storage_name + " = std::move(value);");
		append_line(output, "\t\treturn *this;");
		append_line(output, "\t}");
	}
	append_line(output);
	append_line(output, "private:");
	for (const auto &field : fields) {
		append_line(output, "\t" + field.type_name + " " + field.storage_name + " {};");
	}
	append_line(output, "};");
	append_line(output);
}

std::string setter_pointer_type(const std::string &class_name, const generated_field &field) {
	return class_name + " &(" + class_name + "::*)(" + field.type_name + ")";
}

std::string const_getter_pointer_type(const std::string &class_name, const generated_field &field) {
	return "const " + field.type_name + " &(" + class_name + "::*)() const & noexcept";
}

std::string move_getter_pointer_type(const std::string &class_name, const generated_field &field) {
	return field.type_name + " &&(" + class_name + "::*)() && noexcept";
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
		append_line(output, "\t\t\tstatic_cast<" + setter_pointer_type(qualified_name, generated) + ">(&" +
		                        qualified_name + "::set_" + generated.accessor_name + "),");
		append_line(output, "\t\t\tstatic_cast<" + const_getter_pointer_type(qualified_name, generated) + ">(&" +
		                        qualified_name + "::" + generated.accessor_name + "),");
		append_line(output, "\t\t\tstatic_cast<" + move_getter_pointer_type(qualified_name, generated) + ">(&" +
		                        qualified_name + "::" + generated.accessor_name + "))" +
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
