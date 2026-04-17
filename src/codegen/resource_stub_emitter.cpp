#include "warp/codegen/resource_stub_emitter.hpp"

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
		return type.object_name;
	case schema_type::kind::array_value:
		if (!type.element_type) {
			throw std::invalid_argument("array schema must include an element type");
		}
		return "std::vector<" + cpp_type(*type.element_type) + ">";
	}
	throw std::invalid_argument("unsupported schema type");
}

std::string request_type_name(const api_model &model, const endpoint_model &endpoint) {
	return model.cpp_namespace + "::" + endpoint.request.name;
}

std::string response_type_name(const api_model &model, const endpoint_model &endpoint) {
	return model.cpp_namespace + "::" + endpoint.result_name;
}

void emit_request_contract_traits(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto request_type = request_type_name(model, endpoint);
	append_line(output, "template <>");
	append_line(output, "struct request_contract_traits<" + request_type + "> {");
	append_line(output, "\tstatic parse_result<" + request_type + "> parse(const request &req) {");
	append_line(output, "\t\t" + request_type + " out;");

	for (const auto &parameter : endpoint.request.parameters) {
		const std::string parser = parameter.location == parameter_location::path ? "required_path_param"
		                           : parameter.location == parameter_location::query
		                               ? (parameter.required ? "required_query_param" : "optional_query_param")
		                               : (parameter.required ? "required_header_param" : "optional_header_param");
		const std::string parsed_name = "parsed_" + parameter.member_name;
		append_line(output, "\t\tauto " + parsed_name + " = " + parser + "<" + cpp_type(parameter.type) + ">(req, " +
		                        cpp_string_literal(parameter.source_name) + ");");
		append_line(output, "\t\tif (!" + parsed_name + ".has_value()) {");
		append_line(output, "\t\t\treturn parse_result<" + request_type + ">::failure(" + parsed_name + ".error());");
		append_line(output, "\t\t}");
		append_line(output, "\t\tout." + parameter.member_name + " = std::move(" + parsed_name + ").value();");
	}

	if (endpoint.request.body_type_name.has_value()) {
		append_line(output, "\t\tauto parsed_body = json_body<" + model.cpp_namespace +
		                        "::" + *endpoint.request.body_type_name + ">(req);");
		append_line(output, "\t\tif (!parsed_body.has_value()) {");
		append_line(output, "\t\t\treturn parse_result<" + request_type + ">::failure(parsed_body.error());");
		append_line(output, "\t\t}");
		append_line(output, "\t\tout.body = std::move(parsed_body).value();");
	}

	append_line(output, "\t\treturn parse_result<" + request_type + ">::success(std::move(out));");
	append_line(output, "\t}");
	append_line(output, "};");
	append_line(output);
}

void emit_response_contract_traits(std::string &output, const api_model &model, const endpoint_model &endpoint) {
	const auto response_type = response_type_name(model, endpoint);
	append_line(output, "template <>");
	append_line(output, "struct response_contract_traits<" + response_type + "> {");
	append_line(output, "\tstatic constexpr unsigned status_code = " + response_type + "::status_code;");
	if (endpoint.response.body_type_name.has_value()) {
		append_line(output, "\tstatic constexpr bool has_body = true;");
		append_line(output, "\tstatic const " + model.cpp_namespace + "::" + *endpoint.response.body_type_name +
		                        " &body(const " + response_type + " &value) {");
		append_line(output, "\t\treturn value.body;");
		append_line(output, "\t}");
	} else {
		append_line(output, "\tstatic constexpr bool has_body = false;");
	}
	append_line(output, "};");
	append_line(output);
}

void emit_resource_routes(std::string &output, const api_model &model, const resource_model &resource) {
	append_line(output, "template <typename Service>");
	append_line(output, "class " + resource.routes_class_name + " {");
	append_line(output, "public:");
	append_line(output, "\texplicit " + resource.routes_class_name + "(std::shared_ptr<Service> service)");
	append_line(output, "\t    : service_(std::move(service)) {");
	append_line(output, "\t\tif (!service_) {");
	append_line(output, "\t\t\tthrow std::invalid_argument(\"service must not be null\");");
	append_line(output, "\t\t}");
	append_line(output, "\t}");
	append_line(output);
	append_line(output, "\tvoid register_routes(warp::http::server_builder &builder) const {");
	for (const auto &endpoint : resource.endpoints) {
		const auto request_type = request_type_name(model, endpoint);
		const auto response_type = response_type_name(model, endpoint);
		append_line(output, "\t\tbuilder.route(" + method_expression(endpoint.method) + ", " +
		                        cpp_string_literal(endpoint.path) +
		                        ", [service = service_](warp::request req) -> warp::awaitable<warp::response> {");
		append_line(output, "\t\t\tconst auto version = req.version();");
		append_line(output, "\t\t\tauto typed_request = warp::codegen::parse_http_request<" + request_type + ">(req);");
		append_line(output, "\t\t\tif (!typed_request.has_value()) {");
		append_line(output,
		            "\t\t\t\tauto response = warp::codegen::to_error_response(typed_request.error(), version);");
		append_line(output, "\t\t\t\tco_return response;");
		append_line(output, "\t\t\t}");
		append_line(output, "\t\t\tauto typed_response = co_await warp::codegen::invoke_user_handler<" + response_type +
		                        ">([service, typed_request = std::move(typed_request).value()]() mutable {");
		append_line(output, "\t\t\t\treturn service->" + endpoint.handler_name + "(std::move(typed_request));");
		append_line(output, "\t\t\t});");
		append_line(output, "\t\t\tauto response = warp::codegen::to_http_response(typed_response, version);");
		append_line(output, "\t\t\tco_return response;");
		append_line(output, "\t\t});");
	}
	append_line(output, "\t}");
	append_line(output);
	append_line(output, "private:");
	append_line(output, "\tstd::shared_ptr<Service> service_;");
	append_line(output, "};");
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
	append_line(output, "#include <memory>");
	append_line(output, "#include <stdexcept>");
	append_line(output, "#include <utility>");
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
