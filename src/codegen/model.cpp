#include "codegen/model.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "codegen/model_diagnostics.hpp"
#include "codegen/query_route_analyzer.hpp"
#include "codegen/validation_rule_normalizer.hpp"

namespace warp::codegen {

namespace {

using detail::fail;
using detail::validation_subject;

bool is_cpp_keyword(std::string_view value) {
	static const std::unordered_set<std::string_view> keywords {
	    "alignas",     "alignof",   "and",        "and_eq",    "asm",      "auto",         "bitand",
	    "bitor",       "bool",      "break",      "case",      "catch",    "char",         "char8_t",
	    "char16_t",    "char32_t",  "class",      "compl",     "concept",  "const",        "consteval",
	    "constexpr",   "constinit", "const_cast", "continue",  "co_await", "co_return",    "co_yield",
	    "decltype",    "default",   "delete",     "do",        "double",   "dynamic_cast", "else",
	    "enum",        "explicit",  "export",     "extern",    "false",    "float",        "for",
	    "friend",      "goto",      "if",         "inline",    "int",      "long",         "mutable",
	    "namespace",   "new",       "noexcept",   "not",       "not_eq",   "nullptr",      "operator",
	    "or",          "or_eq",     "private",    "protected", "public",   "register",     "reinterpret_cast",
	    "requires",    "return",    "short",      "signed",    "sizeof",   "static",       "static_assert",
	    "static_cast", "struct",    "switch",     "template",  "this",     "thread_local", "throw",
	    "true",        "try",       "typedef",    "typeid",    "typename", "union",        "unsigned",
	    "using",       "virtual",   "void",       "volatile",  "wchar_t",  "while",        "xor",
	    "xor_eq"};
	return keywords.contains(value);
}

bool is_valid_cpp_identifier(std::string_view value) {
	if (value.empty()) {
		return false;
	}
	const auto first = static_cast<unsigned char>(value.front());
	if (std::isalpha(first) == 0 && value.front() != '_') {
		return false;
	}
	for (char c : value) {
		const auto uc = static_cast<unsigned char>(c);
		if (std::isalnum(uc) == 0 && c != '_') {
			return false;
		}
	}
	return !is_cpp_keyword(value);
}

bool is_valid_cpp_namespace(std::string_view value) {
	// A C++ namespace definition must be a '::'-separated sequence of identifiers,
	// without a leading/trailing '::', and without whitespace.
	if (value.empty()) {
		return false;
	}
	for (char c : value) {
		if (std::isspace(c) != 0) {
			return false;
		}
	}

	std::size_t start = 0;
	while (start < value.size()) {
		const auto next = value.find("::", start);
		const auto part = value.substr(start, next == std::string_view::npos ? value.size() - start : next - start);
		if (!is_valid_cpp_identifier(part)) {
			return false;
		}
		if (next == std::string_view::npos) {
			break;
		}
		start = next + 2;
		if (start >= value.size()) {
			return false; // trailing ::
		}
	}
	return true;
}

bool request_method_forbids_body(http_method method) noexcept {
	switch (method) {
	case http_method::get:
	case http_method::delete_:
		return true;
	case http_method::post:
	case http_method::put:
	case http_method::patch:
		return false;
	}
	return true;
}

std::string canonical_identifier(std::string_view value, std::string_view fallback = "value") {
	std::string out;
	bool previous_was_separator = true;
	for (char c : value) {
		const auto uc = static_cast<unsigned char>(c);
		if (std::isalnum(uc) == 0) {
			if (!previous_was_separator && !out.empty() && out.back() != '_') {
				out.push_back('_');
			}
			previous_was_separator = true;
			continue;
		}
		if (std::isupper(uc) != 0 && !out.empty() && out.back() != '_') {
			out.push_back('_');
		}
		out.push_back(static_cast<char>(std::tolower(uc)));
		previous_was_separator = false;
	}

	while (!out.empty() && out.front() == '_') {
		out.erase(out.begin());
	}
	while (!out.empty() && out.back() == '_') {
		out.pop_back();
	}

	if (out.empty()) {
		out = std::string(fallback);
	}
	if (std::isdigit(static_cast<unsigned char>(out.front())) != 0) {
		out.insert(0, "n_");
	}
	if (is_cpp_keyword(out)) {
		out.push_back('_');
	}
	return out;
}

schema_type::kind primitive_kind(value_kind kind) {
	switch (kind) {
	case value_kind::string_value:
		return schema_type::kind::string_value;
	case value_kind::int64_value:
		return schema_type::kind::int64_value;
	case value_kind::double_value:
		return schema_type::kind::double_value;
	case value_kind::bool_value:
		return schema_type::kind::bool_value;
	case value_kind::object_value:
	case value_kind::array_value:
		break;
	}
	throw std::invalid_argument("expected a primitive schema kind");
}

bool status_forbids_body(int status_code) {
	return (status_code >= 100 && status_code < 200) || status_code == 204 || status_code == 205 || status_code == 304;
}

class local_symbol_table {
public:
	void reserve(std::string_view raw_name, source_span span, std::string_view scope_description) {
		const auto canonical = canonical_identifier(raw_name);
		if (!symbols_.emplace(canonical).second) {
			fail(span, "model.symbol_collision",
			     std::string(scope_description) + " contains a colliding symbol '" + canonical + "'");
		}
	}

	[[nodiscard]] std::string canonical(std::string_view raw_name, source_span span,
	                                    std::string_view scope_description) {
		const auto value = canonical_identifier(raw_name);
		if (!symbols_.emplace(value).second) {
			fail(span, "model.symbol_collision",
			     std::string(scope_description) + " contains a colliding symbol '" + value + "'");
		}
		return value;
	}

private:
	std::unordered_set<std::string> symbols_;
};

class global_symbol_table {
public:
	[[nodiscard]] std::string reserve(std::string_view raw_name, source_span span, std::string_view description) {
		const auto canonical = canonical_identifier(raw_name, "generated_type");
		if (!symbols_.emplace(canonical, span).second) {
			fail(span, "model.symbol_collision",
			     "symbol collision for '" + canonical + "' while defining " + std::string(description));
		}
		return canonical;
	}

private:
	std::unordered_map<std::string, source_span> symbols_;
};

struct route_identity {
	http_method method {http_method::get};
	std::string shape_key;

	[[nodiscard]] std::string key() const {
		return std::string(to_string(method)) + " " + shape_key;
	}
};

struct reserved_route_identity {
	std::string resource_name;
	std::string path;
};

struct model_builder {
	api_model model;
	global_symbol_table global_symbols;
	std::unordered_map<std::string, reserved_route_identity> route_identities;
	detail::validation_rule_normalizer validation_normalizer;
	detail::query_route_analyzer route_analyzer;

	[[nodiscard]] std::string cpp_type_name(const schema_type &type) const {
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
				throw std::invalid_argument("array schema is missing an element type");
			}
			return "std::vector<" + cpp_type_name(*type.element_type) + ">";
		}
		throw std::invalid_argument("unsupported schema type");
	}

	[[nodiscard]] std::string reserve_public_type(std::string_view raw_name, source_span span,
	                                              std::string_view description) {
		return global_symbols.reserve(raw_name, span, description);
	}

	void reserve_route_identity(std::string_view resource_name, http_method method,
	                            const warp::http::route_pattern &route, std::string_view path, source_span span) {
		const route_identity identity {.method = method, .shape_key = route.shape_key};
		const auto key = identity.key();
		const auto [it, inserted] = route_identities.emplace(
		    key, reserved_route_identity {.resource_name = std::string(resource_name), .path = std::string(path)});
		if (!inserted && (it->second.resource_name != resource_name || it->second.path != path)) {
			fail(span, "model.duplicate_route",
			     "duplicate route shape '" + route.shape_key + "' for method " + std::string(to_string(method)));
		}
	}

	[[nodiscard]] schema_type normalize_schema_type(const schema &input, std::string_view hint) {
		if (input.nullable) {
			fail(input.span, "model.nullable_unsupported", "nullable schemas are not supported");
		}
		static_cast<void>(validation_normalizer.normalize(input.validation, input.kind, input.span,
		                                                  validation_subject("schema", hint)));

		switch (input.kind) {
		case value_kind::string_value:
		case value_kind::int64_value:
		case value_kind::double_value:
		case value_kind::bool_value:
			return schema_type {primitive_kind(input.kind)};
		case value_kind::object_value: {
			schema_type type(schema_type::kind::object_value);
			type.object_name = normalize_object_schema(input, input.name.empty() ? std::string(hint) : input.name);
			return type;
		}
		case value_kind::array_value: {
			if (input.element_type == nullptr) {
				fail(input.span, "model.missing_array_item", "array schema is missing an item schema");
			}
			schema_type type(schema_type::kind::array_value);
			type.element_type =
			    std::make_unique<schema_type>(normalize_schema_type(*input.element_type, std::string(hint) + "_item"));
			return type;
		}
		}
		throw std::invalid_argument("unsupported schema kind");
	}

	[[nodiscard]] std::string normalize_object_schema(const schema &input, const std::string &hint) {
		if (input.kind != value_kind::object_value) {
			fail(input.span, "model.expected_object", "object schema expected");
		}

		object_schema_model object;
		object.span = input.span;
		object.name = reserve_public_type(hint, input.span, "schema");

		local_symbol_table member_symbols;
		for (const auto &field : input.fields) {
			if (field.value == nullptr) {
				fail(field.span, "model.missing_field_schema", "field '" + field.name + "' is missing a schema");
			}
			const auto field_validation = validation_normalizer.normalize(
			    field.value->validation, field.value->kind, field.span, validation_subject("field", field.name));
			object.fields.push_back(field_model {
			    .span = field.span,
			    .json_name = field.name,
			    .member_name = member_symbols.canonical(field.name, field.span, "schema '" + object.name + "'"),
			    .type = normalize_schema_type(*field.value, object.name + "_" + field.name),
			    .required = field.required,
			    .validation = std::move(field_validation),
			});
		}

		model.schemas.push_back(std::move(object));
		return model.schemas.back().name;
	}

	[[nodiscard]] request_model build_request_model(const endpoint_spec &endpoint, const std::string &request_name,
	                                                const warp::http::route_pattern &route) {
		request_model request;
		request.span = endpoint.request.span;
		request.name = request_name;
		request.body_mode = endpoint.request.body.has_value() ? http_body_mode::required : http_body_mode::forbidden;

		local_symbol_table member_symbols;
		if (endpoint.request.body.has_value()) {
			if (request_method_forbids_body(endpoint.method)) {
				fail(endpoint.request.body->span, "model.request_body_forbidden",
				     "request body is not allowed for " + std::string(to_string(endpoint.method)) + " endpoints");
			}
			member_symbols.reserve("body", endpoint.request.body->span, "request '" + request_name + "'");
		}

		std::unordered_set<std::string> declared_path_parameters;
		for (const auto &parameter : endpoint.request.parameters) {
			if (!is_primitive(parameter.kind)) {
				fail(parameter.span, "model.invalid_parameter_kind", "request parameters must be primitive scalars");
			}
			if (parameter.location == parameter_location::path && !parameter.required) {
				fail(parameter.span, "model.optional_path_parameter", "path parameters cannot be optional");
			}
			if (parameter.location == parameter_location::path) {
				if (!declared_path_parameters.insert(parameter.name).second) {
					fail(parameter.span, "model.duplicate_path_parameter",
					     "duplicate path parameter declaration '" + parameter.name + "'");
				}
			}
			request.parameters.push_back(parameter_model {
			    .span = parameter.span,
			    .source_name = parameter.name,
			    .member_name =
			        member_symbols.canonical(parameter.name, parameter.span, "request '" + request_name + "'"),
			    .location = parameter.location,
			    .type = schema_type {primitive_kind(parameter.kind)},
			    .required = parameter.required,
			    .validation = validation_normalizer.normalize(parameter.validation, parameter.kind, parameter.span,
			                                                  validation_subject("parameter", parameter.name)),
			});
		}

		std::unordered_set<std::string> route_path_parameters;
		for (const auto &segment : route.segments) {
			if (segment.kind == warp::http::route_segment_kind::parameter) {
				route_path_parameters.insert(segment.text);
				if (!declared_path_parameters.contains(segment.text)) {
					fail(endpoint.path_span, "model.missing_path_parameter",
					     "route path parameter '" + segment.text + "' is missing from the request parameter list");
				}
			}
		}
		for (const auto &declared : declared_path_parameters) {
			if (!route_path_parameters.contains(declared)) {
				fail(endpoint.path_span, "model.unused_path_parameter",
				     "path parameter '" + declared + "' is declared but not present in route path");
			}
		}

		if (endpoint.request.body.has_value()) {
			request.body_type_name =
			    cpp_type_name(normalize_schema_type(*endpoint.request.body, request_name + "_body"));
		}
		return request;
	}

	[[nodiscard]] response_model build_response_model(const endpoint_spec &endpoint, const std::string &response_name) {
		if (endpoint.response.status_code < 100 || endpoint.response.status_code > 599) {
			fail(endpoint.response.status_span.line == 0 ? endpoint.response.span : endpoint.response.status_span,
			     "model.invalid_status", "response status code must be in the range [100, 599]");
		}

		response_model response;
		response.span = endpoint.response.span;
		response.status_span = endpoint.response.status_span;
		response.status_code = endpoint.response.status_code;
		response.body_mode = endpoint.response.body.has_value() ? http_body_mode::required : http_body_mode::forbidden;

		if (status_forbids_body(response.status_code) && endpoint.response.body.has_value()) {
			fail(endpoint.response.status_span.line == 0 ? endpoint.response.span : endpoint.response.status_span,
			     "model.status_body_forbidden",
			     "response status " + std::to_string(response.status_code) + " must not declare a body");
		}

		if (endpoint.response.body.has_value()) {
			response.body_type_name =
			    cpp_type_name(normalize_schema_type(*endpoint.response.body, response_name + "_body"));
		}
		return response;
	}

	resource_model build_resource_model(const resource_spec &resource) {
		resource_model resource_model;
		resource_model.span = resource.span;
		resource_model.name = resource.name;
		resource_model.routes_class_name =
		    reserve_public_type(resource.name + "_api_routes", resource.name_span, "resource routes");

		local_symbol_table handler_symbols;
		std::unordered_map<std::string, std::size_t> route_group_indices;
		for (const auto &endpoint : resource.endpoints) {
			warp::http::route_pattern route;
			try {
				route = warp::http::parse_route_pattern(endpoint.path);
			} catch (const std::invalid_argument &ex) {
				fail(endpoint.path_span, "model.invalid_route", ex.what());
			}
			reserve_route_identity(resource.name, endpoint.method, route, endpoint.path, endpoint.path_span);

			endpoint_model endpoint_model;
			endpoint_model.span = endpoint.span;
			endpoint_model.resource_name = resource.name;
			endpoint_model.endpoint_name = endpoint.name;
			endpoint_model.method = endpoint.method;
			endpoint_model.path = endpoint.path;
			endpoint_model.route = std::move(route);

			const auto prefix = resource.name + "_" + endpoint.name;
			endpoint_model.request_name = reserve_public_type(prefix + "_request", endpoint.span, "request contract");
			endpoint_model.result_name = reserve_public_type(prefix + "_response", endpoint.span, "response contract");
			endpoint_model.handler_name = handler_symbols.canonical(
			    endpoint.name, endpoint.name_span.line == 0 ? endpoint.span : endpoint.name_span,
			    "resource '" + resource.name + "'");
			endpoint_model.request = build_request_model(endpoint, endpoint_model.request_name, endpoint_model.route);
			endpoint_model.response = build_response_model(endpoint, endpoint_model.result_name);
			endpoint_model.query_route = route_analyzer.build_query_route(
			    endpoint_model.request, reserve_public_type(prefix + "_query_route", endpoint.span, "query route spec"),
			    endpoint.span);

			const auto endpoint_index = resource_model.endpoints.size();
			resource_model.endpoints.push_back(std::move(endpoint_model));

			const auto group_key = std::string(to_string(endpoint.method)) + " " + resource_model.endpoints.back().path;
			auto [group_it, inserted] = route_group_indices.emplace(group_key, resource_model.route_groups.size());
			if (inserted) {
				resource_model.route_groups.push_back(route_group_model {
				    .method = resource_model.endpoints.back().method,
				    .path = resource_model.endpoints.back().path,
				});
			}
			resource_model.route_groups.at(group_it->second).endpoint_indices.push_back(endpoint_index);
		}

		route_analyzer.validate_route_groups(resource_model);

		return resource_model;
	}
};

} // namespace

bool validation_rules::empty() const noexcept {
	return !min.has_value() && !max.has_value() && !min_length.has_value() && !max_length.has_value();
}

schema_type::schema_type(const schema_type &other) : type(other.type), object_name(other.object_name) {
	if (other.element_type) {
		element_type = std::make_unique<schema_type>(*other.element_type);
	}
}

schema_type &schema_type::operator=(const schema_type &other) {
	if (this == &other) {
		return *this;
	}
	type = other.type;
	object_name = other.object_name;
	element_type.reset();
	if (other.element_type) {
		element_type = std::make_unique<schema_type>(*other.element_type);
	}
	return *this;
}

api_model build_api_model(const spec_ast &spec, std::string_view namespace_override) {
	model_builder builder;
	// use spec namespace if not provided via cli flag, otherwise its a spec error
	builder.model.cpp_namespace = namespace_override.empty() ? spec.cpp_namespace : std::string(namespace_override);
	if (builder.model.cpp_namespace.empty()) {
		fail(spec.namespace_span.line == 0 ? spec.span : spec.namespace_span, "model.invalid_namespace",
		     "C++ namespace cannot be empty");
	}

	if (!is_valid_cpp_namespace(builder.model.cpp_namespace)) {
		fail(spec.namespace_span.line == 0 ? spec.span : spec.namespace_span, "model.invalid_namespace",
		     "C++ namespace must be a '::'-separated list of valid identifiers");
	}

	for (const auto &resource : spec.resources) {
		builder.model.resources.push_back(builder.build_resource_model(resource));
	}

	return builder.model;
}

} // namespace warp::codegen
