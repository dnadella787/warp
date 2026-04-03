#include "warp/codegen/model.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace warp::codegen {

namespace {

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

std::string to_snake_case(std::string_view value, std::string_view fallback = "value") {
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

std::string sanitize_identifier(const std::string &raw, std::set<std::string> &taken) {
	std::string value;
	value.reserve(raw.size() + 4);

	bool previous_was_separator = true;
	for (char c : raw) {
		const auto uc = static_cast<unsigned char>(c);
		if (std::isalnum(uc) != 0) {
			if (std::isupper(uc) != 0 && !value.empty() && value.back() != '_') {
				value.push_back('_');
			}
			value.push_back(static_cast<char>(std::tolower(uc)));
			previous_was_separator = false;
			continue;
		}
		if (!previous_was_separator && !value.empty() && value.back() != '_') {
			value.push_back('_');
		}
		previous_was_separator = true;
	}

	while (!value.empty() && value.front() == '_') {
		value.erase(value.begin());
	}
	while (!value.empty() && value.back() == '_') {
		value.pop_back();
	}

	if (value.empty()) {
		value = "value";
	}
	if (std::isdigit(static_cast<unsigned char>(value.front())) != 0) {
		value.insert(value.begin(), '_');
	}
	if (is_cpp_keyword(value)) {
		value.push_back('_');
	}

	std::string candidate = value;
	std::size_t suffix = 1;
	while (taken.contains(candidate)) {
		candidate = value + "_" + std::to_string(suffix);
		++suffix;
	}
	taken.insert(candidate);
	return candidate;
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

bool types_equal(const schema_type &lhs, const schema_type &rhs) {
	if (lhs.type != rhs.type || lhs.object_name != rhs.object_name) {
		return false;
	}
	if (static_cast<bool>(lhs.element_type) != static_cast<bool>(rhs.element_type)) {
		return false;
	}
	if (lhs.element_type && rhs.element_type && !types_equal(*lhs.element_type, *rhs.element_type)) {
		return false;
	}
	return true;
}

bool schemas_equal(const object_schema_model &lhs, const object_schema_model &rhs) {
	if (lhs.name != rhs.name || lhs.fields.size() != rhs.fields.size()) {
		return false;
	}
	for (std::size_t index = 0; index < lhs.fields.size(); ++index) {
		const auto &left = lhs.fields[index];
		const auto &right = rhs.fields[index];
		if (left.json_name != right.json_name || left.member_name != right.member_name ||
		    left.required != right.required || !types_equal(left.type, right.type)) {
			return false;
		}
	}
	return true;
}

std::vector<std::string> extract_path_parameters(std::string_view path) {
	std::vector<std::string> parameters;
	std::size_t cursor = 0;
	while (cursor < path.size()) {
		const auto open = path.find('{', cursor);
		if (open == std::string_view::npos) {
			break;
		}
		const auto close = path.find('}', open + 1);
		if (close == std::string_view::npos || close == open + 1) {
			throw std::invalid_argument("route path contains an invalid path parameter placeholder");
		}
		parameters.emplace_back(path.substr(open + 1, close - open - 1));
		cursor = close + 1;
	}
	return parameters;
}

struct model_builder {
	api_model model;
	std::set<std::string> used_type_names;

	[[nodiscard]] std::string unique_type_name(const std::string &raw, bool explicit_name = false) {
		const auto base = to_snake_case(raw, "generated_type");
		if (!used_type_names.contains(base)) {
			used_type_names.insert(base);
			return base;
		}
		if (explicit_name) {
			return base;
		}
		std::size_t suffix = 2;
		for (;;) {
			auto candidate = base + std::to_string(suffix);
			if (!used_type_names.contains(candidate)) {
				used_type_names.insert(candidate);
				return candidate;
			}
			++suffix;
		}
	}

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

	[[nodiscard]] schema_type normalize_schema_type(const schema &input, const std::string &hint) {
		if (input.nullable) {
			throw std::invalid_argument("nullable schemas are not supported yet");
		}

		switch (input.kind) {
		case value_kind::string_value:
		case value_kind::int64_value:
		case value_kind::double_value:
		case value_kind::bool_value:
			return schema_type {primitive_kind(input.kind)};
		case value_kind::object_value: {
			schema_type type(schema_type::kind::object_value);
			type.object_name =
			    normalize_object_schema(input, input.name.empty() ? hint : input.name, !input.name.empty());
			return type;
		}
		case value_kind::array_value: {
			if (input.element_type == nullptr) {
				throw std::invalid_argument("array schema is missing an element type");
			}
			schema_type type(schema_type::kind::array_value);
			type.element_type =
			    std::make_unique<schema_type>(normalize_schema_type(*input.element_type, hint + "_item"));
			return type;
		}
		}
		throw std::invalid_argument("unsupported schema kind");
	}

	[[nodiscard]] std::string normalize_object_schema(const schema &input, const std::string &hint,
	                                                  bool explicit_name) {
		if (input.kind != value_kind::object_value) {
			throw std::invalid_argument("object schema expected");
		}

		object_schema_model object;
		object.name = unique_type_name(hint, explicit_name);
		std::set<std::string> member_names;
		for (const auto &field : input.fields) {
			if (field.value == nullptr) {
				throw std::invalid_argument("field '" + field.name + "' is missing a schema");
			}
			object.fields.push_back(field_model {
			    .json_name = field.name,
			    .member_name = sanitize_identifier(field.name, member_names),
			    .type = normalize_schema_type(*field.value, object.name + "_" + field.name),
			    .required = field.required,
			});
		}

		if (explicit_name) {
			for (const auto &existing : model.schemas) {
				if (existing.name == object.name) {
					if (!schemas_equal(existing, object)) {
						throw std::invalid_argument("conflicting schema definition for type '" + object.name + "'");
					}
					return existing.name;
				}
			}
		}

		model.schemas.push_back(std::move(object));
		return model.schemas.back().name;
	}

	[[nodiscard]] request_model build_request_model(const endpoint_spec &endpoint, const std::string &request_name) {
		request_model request;
		request.name = request_name;

		std::set<std::string> member_names;
		if (endpoint.request.body.has_value()) {
			member_names.insert("body");
		}

		std::set<std::string> declared_path_parameters;
		for (const auto &parameter : endpoint.request.parameters) {
			if (parameter.kind == value_kind::object_value || parameter.kind == value_kind::array_value) {
				throw std::invalid_argument("request parameters must use primitive schema kinds");
			}
			if (parameter.location == parameter_location::path && !parameter.required) {
				throw std::invalid_argument("path parameters cannot be optional");
			}
			if (parameter.location == parameter_location::path) {
				declared_path_parameters.insert(parameter.name);
			}
			request.parameters.push_back(parameter_model {
			    .source_name = parameter.name,
			    .member_name = sanitize_identifier(parameter.name, member_names),
			    .location = parameter.location,
			    .type = schema_type {primitive_kind(parameter.kind)},
			    .required = parameter.required,
			});
		}

		const auto path_parameters = extract_path_parameters(endpoint.path);
		for (const auto &path_parameter : path_parameters) {
			if (!declared_path_parameters.contains(path_parameter)) {
				throw std::invalid_argument("route path parameter '" + path_parameter +
				                            "' is missing from the request parameter list");
			}
		}
		for (const auto &declared : declared_path_parameters) {
			if (std::find(path_parameters.begin(), path_parameters.end(), declared) == path_parameters.end()) {
				throw std::invalid_argument("path parameter '" + declared +
				                            "' is declared but not present in route path");
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
			throw std::invalid_argument("response status code must be in the range [100, 599]");
		}

		response_model response;
		response.status_code = endpoint.response.status_code;
		if (endpoint.response.body.has_value()) {
			response.body_type_name =
			    cpp_type_name(normalize_schema_type(*endpoint.response.body, response_name + "_body"));
		}
		return response;
	}
};

} // namespace

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

api_model build_api_model(const api_spec &spec) {
	model_builder builder;

	std::vector<const resource_spec *> resources;
	resources.reserve(spec.resources.size());
	for (const auto &resource : spec.resources) {
		resources.push_back(&resource);
	}
	std::sort(resources.begin(), resources.end(),
	          [](const resource_spec *lhs, const resource_spec *rhs) { return lhs->name < rhs->name; });

	for (const auto *resource : resources) {
		resource_model resource_model;
		resource_model.name = resource->name;
		resource_model.class_name = builder.unique_type_name(resource->name + "_api_base");

		std::vector<const endpoint_spec *> endpoints;
		endpoints.reserve(resource->endpoints.size());
		for (const auto &endpoint : resource->endpoints) {
			endpoints.push_back(&endpoint);
		}
		std::sort(endpoints.begin(), endpoints.end(),
		          [](const endpoint_spec *lhs, const endpoint_spec *rhs) { return lhs->name < rhs->name; });

		std::set<std::string> handler_names;
		for (const auto *endpoint : endpoints) {
			if (endpoint->path.empty() || endpoint->path.front() != '/') {
				throw std::invalid_argument("endpoint path must start with '/'");
			}

			endpoint_model endpoint_model;
			endpoint_model.resource_name = resource->name;
			endpoint_model.endpoint_name = endpoint->name;
			const auto prefix = resource->name + "_" + endpoint->name;
			endpoint_model.request_name = builder.unique_type_name(prefix + "_request");
			endpoint_model.result_name = builder.unique_type_name(prefix + "_response");

			std::set<std::string> local_handler_names;
			endpoint_model.handler_name = sanitize_identifier(endpoint->name, local_handler_names);
			if (!handler_names.insert(endpoint_model.handler_name).second) {
				throw std::invalid_argument("resource '" + resource->name +
				                            "' contains endpoints that normalize to the same handler name");
			}
			endpoint_model.method = endpoint->method;
			endpoint_model.path = endpoint->path;
			endpoint_model.request = builder.build_request_model(*endpoint, endpoint_model.request_name);
			endpoint_model.response = builder.build_response_model(*endpoint, endpoint_model.result_name);
			resource_model.endpoints.push_back(std::move(endpoint_model));
		}

		builder.model.resources.push_back(std::move(resource_model));
	}

	return builder.model;
}

} // namespace warp::codegen
