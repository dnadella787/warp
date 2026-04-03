#include "warp/codegen/resource_stub_emitter.hpp"

#include "warp/codegen/model.hpp"

#include <stdexcept>
#include <string>

namespace warp::codegen {

namespace {

void append_line(std::string &output, const std::string &line = {}) {
	output.append(line);
	output.push_back('\n');
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

std::string request_type_name(const std::string &ns, const endpoint_model &endpoint) {
	return ns + "::" + endpoint.request.name;
}

std::string response_type_name(const std::string &ns, const endpoint_model &endpoint) {
	return ns + "::" + endpoint.result_name;
}

void emit_request_contract_traits(std::string &output, const std::string &ns, const endpoint_model &endpoint) {
	const auto request_type = request_type_name(ns, endpoint);
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
		append_line(output, "\t\tauto " + parsed_name + " = " + parser + "<" + cpp_type(parameter.type) + ">(req, \"" +
		                        parameter.source_name + "\");");
		append_line(output, "\t\tif (!" + parsed_name + ".has_value()) {");
		append_line(output, "\t\t\treturn parse_result<" + request_type + ">::failure(" + parsed_name + ".error());");
		append_line(output, "\t\t}");
		append_line(output, "\t\tout." + parameter.member_name + " = std::move(" + parsed_name + ").value();");
	}

	if (endpoint.request.body_type_name.has_value()) {
		append_line(output,
		            "\t\tauto parsed_body = json_body<" + ns + "::" + *endpoint.request.body_type_name + ">(req);");
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

void emit_response_contract_traits(std::string &output, const std::string &ns, const endpoint_model &endpoint) {
	const auto response_type = response_type_name(ns, endpoint);
	append_line(output, "template <>");
	append_line(output, "struct response_contract_traits<" + response_type + "> {");
	append_line(output, "\tstatic constexpr unsigned status_code = " + response_type + "::status_code;");
	if (endpoint.response.body_type_name.has_value()) {
		append_line(output, "\tstatic constexpr bool has_body = true;");
		append_line(output, "\tstatic const " + ns + "::" + *endpoint.response.body_type_name + " &body(const " +
		                        response_type + " &value) {");
		append_line(output, "\t\treturn value.body;");
		append_line(output, "\t}");
	} else {
		append_line(output, "\tstatic constexpr bool has_body = false;");
	}
	append_line(output, "};");
	append_line(output);
}

void emit_resource_base(std::string &output, const std::string &ns, const resource_model &resource) {
	append_line(output, "template <typename Derived>");
	append_line(output, "class " + resource.class_name + " {");
	append_line(output, "public:");
	append_line(output, "\tvoid register_routes(warp::http::server_builder &builder) {");
	for (const auto &endpoint : resource.endpoints) {
		const auto request_type = request_type_name(ns, endpoint);
		const auto response_type = response_type_name(ns, endpoint);
		append_line(output, "\t\tbuilder.route(" + method_expression(endpoint.method) + ", \"" + endpoint.path +
		                        "\", [this](warp::request req) -> warp::awaitable<warp::response> {");
		append_line(output, "\t\t\tconst auto version = req.version();");
		append_line(output, "\t\t\tconst auto keep_alive = req.keep_alive();");
		append_line(output, "\t\t\tauto typed_request = warp::codegen::parse_http_request<" + request_type + ">(req);");
		append_line(output, "\t\t\tif (!typed_request.has_value()) {");
		append_line(output,
		            "\t\t\t\tauto response = warp::codegen::to_bad_request_response(typed_request.error(), version);");
		append_line(output, "\t\t\t\tresponse.keep_alive(keep_alive);");
		append_line(output, "\t\t\t\tco_return response;");
		append_line(output, "\t\t\t}");
		append_line(output, "\t\t\tauto typed_response = co_await warp::codegen::invoke_user_handler<" + response_type +
		                        ">([this, typed_request = std::move(typed_request).value()]() mutable {");
		append_line(output, "\t\t\t\treturn derived()." + endpoint.handler_name + "(std::move(typed_request));");
		append_line(output, "\t\t\t});");
		append_line(output, "\t\t\tauto response = warp::codegen::to_http_response(typed_response, version);");
		append_line(output, "\t\t\tresponse.keep_alive(keep_alive);");
		append_line(output, "\t\t\tco_return response;");
		append_line(output, "\t\t});");
	}
	append_line(output, "\t}");
	append_line(output);
	append_line(output, "private:");
	append_line(output, "\t[[nodiscard]] Derived &derived() noexcept {");
	append_line(output, "\t\treturn static_cast<Derived &>(*this);");
	append_line(output, "\t}");
	append_line(output, "};");
	append_line(output);
}

} // namespace

std::string resource_stub_emitter::emit_header(const api_spec &spec,
                                               const resource_stub_emitter_options &options) const {
	const std::string namespace_name = options.namespace_name.empty() ? spec.cpp_namespace : options.namespace_name;
	if (namespace_name.empty()) {
		throw std::invalid_argument("namespace_name cannot be empty");
	}
	if (options.include_data_header && options.data_header_include.empty()) {
		throw std::invalid_argument("data_header_include cannot be empty");
	}

	const auto model = build_api_model(spec);

	std::string output;
	output.reserve(8192);

	append_line(output, "#pragma once");
	append_line(output);
	if (options.include_data_header) {
		append_line(output, "#include \"" + options.data_header_include + "\"");
	}
	append_line(output, "#include \"warp/codegen/http_adapter.hpp\"");
	append_line(output);
	append_line(output, "namespace warp::codegen {");
	append_line(output);

	for (const auto &resource : model.resources) {
		for (const auto &endpoint : resource.endpoints) {
			emit_request_contract_traits(output, namespace_name, endpoint);
			emit_response_contract_traits(output, namespace_name, endpoint);
		}
	}

	append_line(output, "} // namespace warp::codegen");
	append_line(output);
	append_line(output, "namespace " + namespace_name + " {");
	append_line(output);

	for (const auto &resource : model.resources) {
		emit_resource_base(output, namespace_name, resource);
	}

	append_line(output, "} // namespace " + namespace_name);
	return output;
}

} // namespace warp::codegen
