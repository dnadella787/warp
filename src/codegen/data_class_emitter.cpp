#include "warp/codegen/data_class_emitter.hpp"

#include "warp/codegen/model.hpp"

#include <stdexcept>
#include <set>
#include <string>
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

void emit_object_schema(std::string &output, const object_schema_model &schema) {
	append_line(output, "struct " + schema.name + " {");
	for (const auto &field : schema.fields) {
		append_line(output, "\t" + field_cpp_type(field.type, field.required) + " " + field.member_name + " {};");
	}
	append_line(output, "};");
	append_line(output);

	append_line(output, "inline " + schema.name + " tag_invoke(boost::json::value_to_tag<" + schema.name +
	                        ">, const boost::json::value &value) {");
	append_line(output, "\tconst auto &obj = value.as_object();");
	append_line(output, "\t" + schema.name + " out;");
	for (const auto &field : schema.fields) {
		const auto json_key = escape_string_literal(field.json_name);
		const auto raw_name = "raw_" + field.member_name;
		const auto base_type = cpp_type(field.type);
		append_line(output, "\tconst auto *" + raw_name + " = obj.if_contains(\"" + json_key + "\");");
		if (field.required) {
			append_line(output, "\tif (" + raw_name + " == nullptr) {");
			append_line(output, "\t\tthrow std::invalid_argument(\"missing required field '" + json_key + "' for " +
			                        schema.name + "\");");
			append_line(output, "\t}");
			append_line(output, "\tout." + field.member_name + " = boost::json::value_to<" + base_type + ">(*" +
			                        raw_name + ");");
		} else {
			append_line(output, "\tif (" + raw_name + " != nullptr) {");
			append_line(output, "\t\tout." + field.member_name + " = boost::json::value_to<" + base_type + ">(*" +
			                        raw_name + ");");
			append_line(output, "\t}");
		}
	}
	append_line(output, "\treturn out;");
	append_line(output, "}");
	append_line(output);

	append_line(output, "inline void tag_invoke(boost::json::value_from_tag,");
	append_line(output, "\t                    boost::json::value &value,");
	append_line(output, "\t                    const " + schema.name + " &input) {");
	append_line(output, "\tboost::json::object obj;");
	for (const auto &field : schema.fields) {
		const auto json_key = escape_string_literal(field.json_name);
		if (field.required) {
			append_line(output,
			            "\tobj[\"" + json_key + "\"] = boost::json::value_from(input." + field.member_name + ");");
		} else {
			append_line(output, "\tif (input." + field.member_name + ".has_value()) {");
			append_line(output,
			            "\t\tobj[\"" + json_key + "\"] = boost::json::value_from(*input." + field.member_name + ");");
			append_line(output, "\t}");
		}
	}
	append_line(output, "\tvalue = std::move(obj);");
	append_line(output, "}");
	append_line(output);
}

const object_schema_model *find_schema(const api_model &model, const std::string &name) {
	for (const auto &schema : model.schemas) {
		if (schema.name == name) {
			return &schema;
		}
	}
	return nullptr;
}

void emit_schema_with_dependencies(std::string &output, const api_model &model, const object_schema_model &schema,
                                   std::set<std::string> &emitted) {
	if (emitted.contains(schema.name)) {
		return;
	}

	for (const auto &field : schema.fields) {
		if (field.type.type == schema_type::kind::object_value) {
			if (const auto *dependency = find_schema(model, field.type.object_name)) {
				emit_schema_with_dependencies(output, model, *dependency, emitted);
			}
		} else if (field.type.type == schema_type::kind::array_value && field.type.element_type &&
		           field.type.element_type->type == schema_type::kind::object_value) {
			if (const auto *dependency = find_schema(model, field.type.element_type->object_name)) {
				emit_schema_with_dependencies(output, model, *dependency, emitted);
			}
		}
	}

	emit_object_schema(output, schema);
	emitted.insert(schema.name);
}

void emit_request_contract(std::string &output, const endpoint_model &endpoint) {
	append_line(output, "struct " + endpoint.request.name + " {");
	for (const auto &parameter : endpoint.request.parameters) {
		append_line(output,
		            "\t" + field_cpp_type(parameter.type, parameter.required) + " " + parameter.member_name + " {};");
	}
	if (endpoint.request.body_type_name.has_value()) {
		append_line(output, "\t" + *endpoint.request.body_type_name + " body {};");
	}
	append_line(output, "};");
	append_line(output);
}

void emit_result_contract(std::string &output, const endpoint_model &endpoint) {
	append_line(output, "struct " + endpoint.result_name + " {");
	append_line(output,
	            "\tstatic constexpr unsigned status_code = " + std::to_string(endpoint.response.status_code) + ";");
	if (endpoint.response.body_type_name.has_value()) {
		append_line(output, "\t" + *endpoint.response.body_type_name + " body {};");
	}
	append_line(output, "};");
	append_line(output);
}

} // namespace

std::string data_class_emitter::emit_header(const api_model &model) const {
	if (model.cpp_namespace.empty()) {
		throw std::invalid_argument("namespace_name cannot be empty");
	}

	std::string output;
	output.reserve(8192);

	append_line(output, "#pragma once");
	append_line(output);
	append_line(output, "#include <boost/json/object.hpp>");
	append_line(output, "#include <boost/json/value.hpp>");
	append_line(output, "#include <boost/json/value_from.hpp>");
	append_line(output, "#include <boost/json/value_to.hpp>");
	append_line(output);
	append_line(output, "#include <cstdint>");
	append_line(output, "#include <optional>");
	append_line(output, "#include <stdexcept>");
	append_line(output, "#include <string>");
	append_line(output, "#include <utility>");
	append_line(output, "#include <vector>");
	append_line(output);
	append_line(output, "namespace " + model.cpp_namespace + " {");
	append_line(output);

	std::set<std::string> emitted_schemas;
	for (const auto &schema : model.schemas) {
		emit_schema_with_dependencies(output, model, schema, emitted_schemas);
	}

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			emit_request_contract(output, endpoint);
			emit_result_contract(output, endpoint);
		}
	}

	append_line(output, "} // namespace " + model.cpp_namespace);
	return output;
}

} // namespace warp::codegen
