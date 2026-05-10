#include "codegen/spec_ast_decoder.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <exception>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "codegen/spec_parser.hpp"

namespace warp::codegen::detail {

namespace {
const yaml_node &expect_kind(const yaml_node &node, yaml_node::kind kind, std::string_view context);

void reject_unknown_keys(const yaml_node &node, std::initializer_list<std::string_view> allowed,
                         std::string_view context) {
	expect_kind(node, yaml_node::kind::map, context);
	const std::unordered_set allowed_keys(allowed.begin(), allowed.end());
	for (const auto &entry : node.map_values) {
		if (!allowed_keys.contains(entry.first)) {
			throw spec_error(source_span {.line = entry.second.line, .column = node.column}, "spec.unknown_key",
			                 std::string(context) + " contains unknown key '" + entry.first + "'");
		}
	}
}

// get a value for a specific key in a map node, return null otherwise
const yaml_node *find_key(const yaml_node &node, std::string_view key) {
	if (node.type != yaml_node::kind::map) {
		return nullptr;
	}
	for (const auto &entry : node.map_values) {
		if (entry.first == key) {
			return &entry.second;
		}
	}
	return nullptr;
}

const yaml_node &required_key(const yaml_node &node, std::string_view key) {
	if (const auto *value = find_key(node, key)) {
		return *value;
	}
	throw spec_error(node.line, node.column, "missing required key '" + std::string(key) + "'");
}

const yaml_node &expect_kind(const yaml_node &node, yaml_node::kind kind, std::string_view context) {
	if (node.type != kind) {
		throw spec_error(node.line, node.column,
		                 std::string(context) + " must be a " +
		                     (kind == yaml_node::kind::map    ? "mapping"
		                      : kind == yaml_node::kind::list ? "list"
		                                                      : "scalar"));
	}
	return node;
}

std::string parse_string(const yaml_node &node, std::string_view context) {
	expect_kind(node, yaml_node::kind::scalar, context);
	return node.scalar;
}

bool parse_bool(const yaml_node &node, std::string_view context) {
	const auto value = parse_string(node, context);
	if (value == "true") {
		return true;
	}
	if (value == "false") {
		return false;
	}
	throw spec_error(node.line, node.column, std::string(context) + " must be 'true' or 'false'");
}

std::optional<std::int64_t> parse_int64_literal(std::string_view text) {
	std::int64_t value = 0;
	const auto *begin = text.data();
	const auto *end = text.data() + text.size();
	const auto result = std::from_chars(begin, end, value);
	if (result.ec == std::errc {} && result.ptr == end) {
		return value;
	}
	return std::nullopt;
}

std::optional<double> parse_double_literal(std::string_view text) {
	std::size_t offset = 0;
	try {
		const auto value = std::stod(std::string(text), &offset);
		if (offset == text.size()) {
			return value;
		}
	} catch (const std::exception &) {
	}
	return std::nullopt;
}

numeric_validation_value parse_numeric_validation_value(const yaml_node &node, std::string_view context) {
	const auto value = parse_string(node, context);
	if (const auto parsed = parse_int64_literal(value); parsed.has_value()) {
		return *parsed;
	}
	if (const auto parsed = parse_double_literal(value); parsed.has_value()) {
		return *parsed;
	}
	throw spec_error(node.line, node.column, std::string(context) + " must be a numeric scalar");
}

std::size_t parse_length_validation_value(const yaml_node &node, std::string_view context) {
	const auto value = parse_string(node, context);
	unsigned long long parsed = 0;
	const auto *begin = value.data();
	const auto *end = value.data() + value.size();
	const auto result = std::from_chars(begin, end, parsed);
	if (result.ec != std::errc {} || result.ptr != end ||
	    parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
		throw spec_error(node.line, node.column, std::string(context) + " must be a non-negative integer");
	}
	return static_cast<std::size_t>(parsed);
}

validation_rule_spec parse_validation_rules(const yaml_node &node) {
	validation_rule_spec rules;
	if (const auto *min = find_key(node, "min")) {
		rules.min_span = span_of(*min);
		rules.min = parse_numeric_validation_value(*min, "validation rule 'min'");
	}
	if (const auto *max = find_key(node, "max")) {
		rules.max_span = span_of(*max);
		rules.max = parse_numeric_validation_value(*max, "validation rule 'max'");
	}
	if (const auto *min_length = find_key(node, "min_length")) {
		rules.min_length_span = span_of(*min_length);
		rules.min_length = parse_length_validation_value(*min_length, "validation rule 'min_length'");
	}
	if (const auto *max_length = find_key(node, "max_length")) {
		rules.max_length_span = span_of(*max_length);
		rules.max_length = parse_length_validation_value(*max_length, "validation rule 'max_length'");
	}
	return rules;
}

int parse_int(const yaml_node &node, std::string_view context) {
	const auto value = parse_string(node, context);
	try {
		return std::stoi(value);
	} catch (const std::exception &) {
		throw spec_error(node.line, node.column, std::string(context) + " must be an integer");
	}
}

parameter_location parse_location(const yaml_node &node) {
	const auto value = parse_string(node, "parameter location");
	if (value == "path") {
		return parameter_location::path;
	}
	if (value == "query") {
		return parameter_location::query;
	}
	if (value == "header") {
		return parameter_location::header;
	}
	throw spec_error(node.line, node.column, "parameter location must be one of: path, query, header");
}

http_method parse_method(const yaml_node &node) {
	std::string value = parse_string(node, "endpoint method");
	std::transform(value.begin(), value.end(), value.begin(),
	               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	if (value == "GET") {
		return http_method::get;
	}
	if (value == "POST") {
		return http_method::post;
	}
	if (value == "PUT") {
		return http_method::put;
	}
	if (value == "PATCH") {
		return http_method::patch;
	}
	if (value == "DELETE") {
		return http_method::delete_;
	}
	throw spec_error(node.line, node.column, "endpoint method must be one of: GET, POST, PUT, PATCH, DELETE");
}

value_kind parse_scalar_kind(const yaml_node &node, std::string_view context) {
	const auto value = parse_string(node, context);
	if (value == "string") {
		return value_kind::string_value;
	}
	if (value == "int64" || value == "integer") {
		return value_kind::int64_value;
	}
	if (value == "double" || value == "number") {
		return value_kind::double_value;
	}
	if (value == "bool" || value == "boolean") {
		return value_kind::bool_value;
	}
	throw spec_error(node.line, node.column, std::string(context) + " must be one of: string, int64, double, bool");
}

std::string derive_endpoint_name(http_method method, std::string_view path) {
	std::string out = std::string(to_string(method));
	std::transform(out.begin(), out.end(), out.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	bool needs_separator = true;
	std::size_t start = 0;
	while (start < path.size()) {
		const auto end = path.find('/', start);
		const auto token = path.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!token.empty()) {
			if (needs_separator) {
				out.push_back('_');
			}
			for (char c : token) {
				if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
					out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
				}
			}
			needs_separator = true;
		}
		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	return out.empty() ? "endpoint" : out;
}

schema parse_schema(const yaml_node &node, bool allow_schema_name = true);

schema parse_object_schema(const yaml_node &node, bool allow_schema_name) {
	reject_unknown_keys(node,
	                    allow_schema_name
	                        ? std::initializer_list<std::string_view> {"type", "name", "nullable", "fields", "min",
	                                                                   "max", "min_length", "max_length"}
	                        : std::initializer_list<std::string_view> {"type", "nullable", "fields", "min", "max",
	                                                                   "min_length", "max_length"},
	                    "object schema");
	schema parsed = schema::object();
	parsed.span = span_of(node);
	parsed.validation = parse_validation_rules(node);
	if (allow_schema_name) {
		if (const auto *name = find_key(node, "name")) {
			parsed.name = parse_string(*name, "schema name");
		}
	}
	if (const auto *nullable = find_key(node, "nullable")) {
		parsed.nullable = parse_bool(*nullable, "schema nullable");
	}

	const auto &fields = expect_kind(required_key(node, "fields"), yaml_node::kind::list, "object schema fields");
	for (const auto &field_node : fields.list_values) {
		reject_unknown_keys(field_node,
		                    {"name", "required", "type", "nullable", "fields", "items", "schema", "min", "max",
		                     "min_length", "max_length"},
		                    "schema field");
		expect_kind(field_node, yaml_node::kind::map, "schema field");
		const auto field_name = parse_string(required_key(field_node, "name"), "field name");
		bool required = true;
		if (const auto *field_required = find_key(field_node, "required")) {
			required = parse_bool(*field_required, "field required");
		}
		if (const auto *field_schema = find_key(field_node, "schema")) {
			if (find_key(field_node, "type") != nullptr || find_key(field_node, "nullable") != nullptr ||
			    find_key(field_node, "fields") != nullptr || find_key(field_node, "items") != nullptr ||
			    find_key(field_node, "min") != nullptr || find_key(field_node, "max") != nullptr ||
			    find_key(field_node, "min_length") != nullptr || find_key(field_node, "max_length") != nullptr) {
				throw spec_error(span_of(field_node), "spec.invalid_field_schema",
				                 "schema field cannot mix 'schema' with inline schema keys");
			}
			parsed.append_field(span_of(field_node), field_name, parse_schema(*field_schema), required);
		} else {
			yaml_node inline_schema {
			    .type = yaml_node::kind::map, .line = field_node.line, .column = field_node.column};
			for (const auto &entry : field_node.map_values) {
				if (entry.first == "name" || entry.first == "required") {
					continue;
				}
				inline_schema.map_values.push_back(entry);
			}
			parsed.append_field(span_of(field_node), field_name, parse_schema(inline_schema, false), required);
		}
	}
	return parsed;
}

schema parse_schema(const yaml_node &node, bool allow_schema_name) {
	if (node.type == yaml_node::kind::scalar) {
		schema parsed;
		switch (parse_scalar_kind(node, "schema type")) {
		case value_kind::string_value:
			parsed = schema::string();
			break;
		case value_kind::int64_value:
			parsed = schema::int64();
			break;
		case value_kind::double_value:
			parsed = schema::number();
			break;
		case value_kind::bool_value:
			parsed = schema::boolean();
			break;
		case value_kind::object_value:
		case value_kind::array_value:
			break;
		}
		parsed.span = span_of(node);
		return parsed;
	}
	expect_kind(node, yaml_node::kind::map, "schema");
	const auto &type_node = required_key(node, "type");
	const auto type_name = parse_string(type_node, "schema type");

	if (type_name == "object") {
		return parse_object_schema(node, allow_schema_name);
	}
	if (type_name == "array") {
		reject_unknown_keys(node,
		                    allow_schema_name
		                        ? std::initializer_list<std::string_view> {"type", "name", "nullable", "items", "min",
		                                                                   "max", "min_length", "max_length"}
		                        : std::initializer_list<std::string_view> {"type", "nullable", "items", "min", "max",
		                                                                   "min_length", "max_length"},
		                    "array schema");
		schema parsed(value_kind::array_value);
		parsed.span = span_of(node);
		parsed.validation = parse_validation_rules(node);
		if (allow_schema_name) {
			if (const auto *name = find_key(node, "name")) {
				parsed.name = parse_string(*name, "schema name");
			}
		}
		if (const auto *nullable = find_key(node, "nullable")) {
			parsed.nullable = parse_bool(*nullable, "schema nullable");
		}
		const auto &items = required_key(node, "items");
		parsed.set_element(span_of(items), parse_schema(items));
		return parsed;
	}

	reject_unknown_keys(node,
	                    allow_schema_name ? std::initializer_list<std::string_view> {"type", "name", "nullable", "min",
	                                                                                 "max", "min_length", "max_length"}
	                                      : std::initializer_list<std::string_view> {"type", "nullable", "min", "max",
	                                                                                 "min_length", "max_length"},
	                    "schema");
	schema parsed(value_kind::string_value);
	switch (parse_scalar_kind(type_node, "schema type")) {
	case value_kind::string_value:
		parsed = schema::string();
		break;
	case value_kind::int64_value:
		parsed = schema::int64();
		break;
	case value_kind::double_value:
		parsed = schema::number();
		break;
	case value_kind::bool_value:
		parsed = schema::boolean();
		break;
	case value_kind::object_value:
	case value_kind::array_value:
		break;
	}
	parsed.span = span_of(node);
	parsed.validation = parse_validation_rules(node);
	if (allow_schema_name) {
		if (const auto *name = find_key(node, "name")) {
			parsed.name = parse_string(*name, "schema name");
		}
	}
	if (const auto *nullable = find_key(node, "nullable")) {
		parsed.nullable = parse_bool(*nullable, "schema nullable");
	}
	return parsed;
}

int parse_status_code(const yaml_node &node) {
	const auto value = parse_string(node, "response status");
	if (!value.empty() && std::all_of(value.begin(), value.end(),
	                                  [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
		return parse_int(node, "response status");
	}
	if (value.size() >= 3 && std::isdigit(static_cast<unsigned char>(value[0])) != 0 &&
	    std::isdigit(static_cast<unsigned char>(value[1])) != 0 &&
	    std::isdigit(static_cast<unsigned char>(value[2])) != 0 &&
	    (value.size() == 3 || std::isspace(static_cast<unsigned char>(value[3])) != 0)) {
		try {
			return std::stoi(value.substr(0, 3));
		} catch (const std::exception &) {
		}
	}
	throw spec_error(span_of(node), "spec.invalid_status",
	                 "response status must be a 3-digit code or '<code> <reason>' string");
}

parameter_spec parse_parameter(const yaml_node &node) {
	reject_unknown_keys(node, {"name", "in", "location", "type", "required", "min", "max", "min_length", "max_length"},
	                    "parameter");
	expect_kind(node, yaml_node::kind::map, "parameter");
	parameter_spec parameter;
	parameter.span = span_of(node);
	parameter.name = parse_string(required_key(node, "name"), "parameter name");
	if (find_key(node, "in") != nullptr && find_key(node, "location") != nullptr) {
		throw spec_error(span_of(node), "spec.duplicate_semantic_key",
		                 "parameter cannot specify both 'in' and 'location'");
	}
	if (const auto *location = find_key(node, "in")) {
		parameter.location = parse_location(*location);
	} else {
		parameter.location = parse_location(required_key(node, "location"));
	}
	parameter.kind = parse_scalar_kind(required_key(node, "type"), "parameter type");
	parameter.required = true;
	if (const auto *required = find_key(node, "required")) {
		parameter.required = parse_bool(*required, "parameter required");
	}
	parameter.validation = parse_validation_rules(node);
	return parameter;
}

request_spec parse_request(const yaml_node &node) {
	request_spec request;
	request.span = span_of(node);
	if (node.type != yaml_node::kind::map ||
	    (find_key(node, "parameters") == nullptr && find_key(node, "body") == nullptr)) {
		request.body = parse_schema(node);
		return request;
	}
	reject_unknown_keys(node, {"parameters", "body"}, "request");
	expect_kind(node, yaml_node::kind::map, "request");
	if (const auto *parameters = find_key(node, "parameters")) {
		expect_kind(*parameters, yaml_node::kind::list, "request parameters");
		for (const auto &parameter_node : parameters->list_values) {
			request.parameters.push_back(parse_parameter(parameter_node));
		}
	}
	if (const auto *body = find_key(node, "body")) {
		request.body = parse_schema(*body);
	}
	return request;
}

response_spec parse_response(const yaml_node &node) {
	response_spec response;
	response.span = span_of(node);
	if (node.type != yaml_node::kind::map ||
	    (find_key(node, "status") == nullptr && find_key(node, "body") == nullptr)) {
		response.body = parse_schema(node);
		return response;
	}
	reject_unknown_keys(node, {"status", "body"}, "response");
	expect_kind(node, yaml_node::kind::map, "response");
	if (const auto *status = find_key(node, "status")) {
		response.status_span = span_of(*status);
		response.status_code = parse_status_code(*status);
	}
	if (response.status_code < 100 || response.status_code > 599) {
		throw spec_error(response.status_span.line == 0 ? span_of(node) : response.status_span, "spec.invalid_status",
		                 "response status must be between 100 and 599");
	}
	if (const auto *body = find_key(node, "body")) {
		response.body = parse_schema(*body);
	}
	return response;
}

endpoint_spec parse_endpoint(const yaml_node &node) {
	reject_unknown_keys(node, {"name", "method", "path", "request", "response"}, "endpoint");
	expect_kind(node, yaml_node::kind::map, "endpoint");
	endpoint_spec endpoint;
	endpoint.span = span_of(node);
	if (const auto *method = find_key(node, "method")) {
		endpoint.method_span = span_of(*method);
		endpoint.method = parse_method(*method);
	}
	const auto &path = required_key(node, "path");
	endpoint.path_span = span_of(path);
	endpoint.path = parse_string(path, "endpoint path");
	if (const auto *name = find_key(node, "name")) {
		endpoint.name_span = span_of(*name);
		endpoint.name = parse_string(*name, "endpoint name");
	} else {
		endpoint.name = derive_endpoint_name(endpoint.method, endpoint.path);
	}
	if (const auto *request = find_key(node, "request")) {
		endpoint.request = parse_request(*request);
	}
	if (const auto *response = find_key(node, "response")) {
		endpoint.response = parse_response(*response);
	}
	return endpoint;
}

resource_spec parse_resource(const yaml_node &node) {
	reject_unknown_keys(node, {"name", "endpoints"}, "resource");
	expect_kind(node, yaml_node::kind::map, "resource");
	resource_spec resource;
	resource.span = span_of(node);
	const auto &name = required_key(node, "name");
	resource.name_span = span_of(name);
	resource.name = parse_string(name, "resource name");
	const auto &endpoints = expect_kind(required_key(node, "endpoints"), yaml_node::kind::list, "resource endpoints");
	for (const auto &endpoint_node : endpoints.list_values) {
		resource.endpoints.push_back(parse_endpoint(endpoint_node));
	}
	return resource;
}

} // namespace

spec_ast decode_spec_ast(const yaml_node &document) {
	spec_ast spec;
	spec.span = span_of(document);

	expect_kind(document, yaml_node::kind::map, "root YAML document");
	reject_unknown_keys(document, {"name", "namespace", "cpp_namespace", "resources", "endpoints"},
	                    "root YAML document");

	const auto *namespace_node = find_key(document, "namespace");
	const auto *cpp_namespace_node = find_key(document, "cpp_namespace");
	// cannot allow both namespace and cpp_namespace
	if (namespace_node && cpp_namespace_node) {
		throw spec_error(span_of(document), "spec.duplicate_semantic_key",
		                 "root YAML document cannot specify both 'namespace' and 'cpp_namespace'");
	}

	// choose between one or the other, defaults to generated otherwise
	if (namespace_node) {
		spec.namespace_span = span_of(*namespace_node);
		spec.cpp_namespace = parse_string(*namespace_node, "namespace");
	} else if (cpp_namespace_node) {
		spec.namespace_span = span_of(*cpp_namespace_node);
		spec.cpp_namespace = parse_string(*cpp_namespace_node, "cpp namespace");
	}

	const auto *resources = find_key(document, "resources");
	const auto *endpoints = find_key(document, "endpoints");
	// only allow one of either resources or endpoints
	if (resources && endpoints) {
		throw spec_error(span_of(document), "spec.ambiguous_root",
		                 "root YAML document cannot contain both 'resources' and 'endpoints'");
	}
	if (resources) {
		if (find_key(document, "name") != nullptr) {
			throw spec_error(span_of(document), "spec.invalid_root_name",
			                 "top-level 'name' is only allowed with top-level 'endpoints'");
		}
		expect_kind(*resources, yaml_node::kind::list, "resources");
		for (const auto &resource_node : resources->list_values) {
			spec.resources.push_back(parse_resource(resource_node));
		}
		return spec;
	}

	if (endpoints) {
		resource_spec resource;
		resource.span = span_of(document);
		resource.name = find_key(document, "name") == nullptr
		                    ? "default"
		                    : parse_string(*find_key(document, "name"), "resource name");
		if (const auto *name = find_key(document, "name")) {
			resource.name_span = span_of(*name);
		}
		expect_kind(*endpoints, yaml_node::kind::list, "endpoints");
		for (const auto &endpoint_node : endpoints->list_values) {
			resource.endpoints.push_back(parse_endpoint(endpoint_node));
		}
		spec.resources.push_back(std::move(resource));
		return spec;
	}

	throw spec_error(document.line, document.column, "root YAML document must contain 'resources' or 'endpoints'");
}

} // namespace warp::codegen::detail
