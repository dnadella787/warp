#include "codegen/model.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "server/router/query_constraint_semantics.hpp"

namespace warp::codegen {

namespace {

[[noreturn]] void fail(source_span span, std::string code, std::string message) {
	throw diagnostic_error(diagnostic {
	    .severity = diagnostic_severity::error, .code = std::move(code), .message = std::move(message), .span = span});
}

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
		if (std::isspace(static_cast<unsigned char>(c)) != 0) {
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

source_span validation_span_or(source_span span, source_span fallback) {
	return span.line == 0 ? fallback : span;
}

[[nodiscard]] std::string validation_subject(std::string_view noun, std::string_view name) {
	return std::string(noun) + " '" + std::string(name) + "'";
}

[[nodiscard]] bool is_validation_value_integral(const numeric_validation_value &value) {
	return std::holds_alternative<std::int64_t>(value);
}

[[nodiscard]] std::int64_t as_int64_validation_value(const numeric_validation_value &value) {
	return std::get<std::int64_t>(value);
}

[[nodiscard]] double as_double_validation_value(const numeric_validation_value &value) {
	if (const auto *integer = std::get_if<std::int64_t>(&value)) {
		return static_cast<double>(*integer);
	}
	return std::get<double>(value);
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

[[nodiscard]] bool ordered_contains(const std::vector<std::string> &values, std::string_view value) {
	return std::find(values.begin(), values.end(), value) != values.end();
}

void append_unique(std::vector<std::string> &values, std::string_view value) {
	if (!ordered_contains(values, value)) {
		values.emplace_back(value);
	}
}

[[nodiscard]] std::optional<warp::http::compiled_query_constraint>
find_query_constraint(const std::vector<warp::http::compiled_query_constraint> &constraints, std::string_view name) {
	for (const auto &constraint : constraints) {
		if (constraint.name == name) {
			return constraint;
		}
	}
	return std::nullopt;
}

[[nodiscard]] bool
query_routes_can_tie_on_score(const std::vector<warp::http::compiled_query_constraint> &lhs,
                              const std::vector<warp::http::compiled_query_constraint> &rhs,
                              const std::vector<std::string_view> &constraint_names, std::size_t index = 0,
                              warp::http::routing_detail::query_constraint_match_score lhs_score = {},
                              warp::http::routing_detail::query_constraint_match_score rhs_score = {}) {
	if (index == constraint_names.size()) {
		return warp::http::routing_detail::query_match_scores_equal(lhs_score, rhs_score);
	}

	const auto lhs_descriptor = find_query_constraint(lhs, constraint_names[index]);
	const auto rhs_descriptor = find_query_constraint(rhs, constraint_names[index]);
	const auto lhs_exact_value = lhs_descriptor.has_value() && lhs_descriptor->value.has_value()
	                                 ? std::string_view(*lhs_descriptor->value)
	                                 : std::string_view {};
	const auto rhs_exact_value = rhs_descriptor.has_value() && rhs_descriptor->value.has_value()
	                                 ? std::string_view(*rhs_descriptor->value)
	                                 : std::string_view {};

	constexpr std::array<warp::http::routing_detail::query_value_state, 4> states {
	    warp::http::routing_detail::query_value_state::absent,
	    warp::http::routing_detail::query_value_state::lhs_exact,
	    warp::http::routing_detail::query_value_state::rhs_exact,
	    warp::http::routing_detail::query_value_state::other_present,
	};

	for (const auto state : states) {
		if (!warp::http::routing_detail::query_constraint_accepts_state(lhs_descriptor, lhs_exact_value,
		                                                                rhs_exact_value, state) ||
		    !warp::http::routing_detail::query_constraint_accepts_state(rhs_descriptor, lhs_exact_value,
		                                                                rhs_exact_value, state)) {
			continue;
		}

		if (query_routes_can_tie_on_score(
		        lhs, rhs, constraint_names, index + 1,
		        warp::http::routing_detail::add_query_match_scores(
		            lhs_score, warp::http::routing_detail::query_constraint_score(lhs_descriptor, state)),
		        warp::http::routing_detail::add_query_match_scores(
		            rhs_score, warp::http::routing_detail::query_constraint_score(rhs_descriptor, state)))) {
			return true;
		}
	}

	return false;
}

[[nodiscard]] std::vector<warp::http::compiled_query_constraint>
effective_query_route_constraints(const query_route_model &query_route, const route_group_model &group) {
	std::vector<warp::http::compiled_query_constraint> constraints;
	constraints.reserve(group.routing_query_parameters.size());
	for (const auto &name : group.routing_query_parameters) {
		if (const auto constraint = find_query_constraint(query_route.constraints, name); constraint.has_value()) {
			constraints.push_back(*constraint);
			continue;
		}
		constraints.push_back(warp::http::compiled_query_constraint {
		    .name = name,
		    .presence = warp::http::query_constraint_presence::forbidden,
		});
	}
	warp::http::detail::sort_compiled_query_constraints(constraints);
	return constraints;
}

[[nodiscard]] bool query_route_specs_overlap(const std::vector<warp::http::compiled_query_constraint> &lhs,
                                             const std::vector<warp::http::compiled_query_constraint> &rhs) {
	std::vector<std::string_view> constraint_names;
	constraint_names.reserve(lhs.size() + rhs.size());
	for (const auto &constraint : lhs) {
		if (std::ranges::find(constraint_names, constraint.name) == constraint_names.end()) {
			constraint_names.push_back(constraint.name);
		}
	}
	for (const auto &constraint : rhs) {
		if (std::ranges::find(constraint_names, constraint.name) == constraint_names.end()) {
			constraint_names.push_back(constraint.name);
		}
	}
	for (const auto &lhs_constraint : lhs) {
		for (const auto &rhs_constraint : rhs) {
			if (lhs_constraint.name == rhs_constraint.name &&
			    !warp::http::routing_detail::query_constraints_can_overlap(lhs_constraint, rhs_constraint)) {
				return false;
			}
		}
	}
	return query_routes_can_tie_on_score(lhs, rhs, constraint_names);
}

struct model_builder {
	api_model model;
	global_symbol_table global_symbols;
	std::unordered_map<std::string, reserved_route_identity> route_identities;

	[[nodiscard]] validation_rules normalize_validation_rules(const validation_rule_spec &input, value_kind kind,
	                                                          source_span fallback_span,
	                                                          std::string_view subject) const {
		validation_rules output;

		auto reject = [&](source_span span, std::string message) {
			fail(validation_span_or(span, fallback_span), "model.invalid_validation_rule", std::move(message));
		};

		switch (kind) {
		case value_kind::string_value:
			if (input.min.has_value()) {
				reject(input.min_span, std::string(subject) + " cannot use numeric rule 'min' with string type");
			}
			if (input.max.has_value()) {
				reject(input.max_span, std::string(subject) + " cannot use numeric rule 'max' with string type");
			}
			output.min_length = input.min_length;
			output.max_length = input.max_length;
			if (output.min_length.has_value() && output.max_length.has_value() &&
			    *output.min_length > *output.max_length) {
				fail(validation_span_or(input.max_length_span, fallback_span), "model.invalid_validation_range",
				     std::string(subject) + " has min_length greater than max_length");
			}
			return output;
		case value_kind::int64_value:
			if (input.min_length.has_value()) {
				reject(input.min_length_span,
				       std::string(subject) + " cannot use string rule 'min_length' with int64 type");
			}
			if (input.max_length.has_value()) {
				reject(input.max_length_span,
				       std::string(subject) + " cannot use string rule 'max_length' with int64 type");
			}
			if (input.min.has_value()) {
				if (!is_validation_value_integral(*input.min)) {
					reject(input.min_span, std::string(subject) + " must use an integer value for rule 'min'");
				}
				output.min = as_int64_validation_value(*input.min);
			}
			if (input.max.has_value()) {
				if (!is_validation_value_integral(*input.max)) {
					reject(input.max_span, std::string(subject) + " must use an integer value for rule 'max'");
				}
				output.max = as_int64_validation_value(*input.max);
			}
			if (output.min.has_value() && output.max.has_value() &&
			    as_int64_validation_value(*output.min) > as_int64_validation_value(*output.max)) {
				fail(validation_span_or(input.max_span, fallback_span), "model.invalid_validation_range",
				     std::string(subject) + " has min greater than max");
			}
			return output;
		case value_kind::double_value:
			if (input.min_length.has_value()) {
				reject(input.min_length_span,
				       std::string(subject) + " cannot use string rule 'min_length' with double type");
			}
			if (input.max_length.has_value()) {
				reject(input.max_length_span,
				       std::string(subject) + " cannot use string rule 'max_length' with double type");
			}
			if (input.min.has_value()) {
				output.min = as_double_validation_value(*input.min);
			}
			if (input.max.has_value()) {
				output.max = as_double_validation_value(*input.max);
			}
			if (output.min.has_value() && output.max.has_value() &&
			    as_double_validation_value(*output.min) > as_double_validation_value(*output.max)) {
				fail(validation_span_or(input.max_span, fallback_span), "model.invalid_validation_range",
				     std::string(subject) + " has min greater than max");
			}
			return output;
		case value_kind::bool_value:
			break;
		case value_kind::object_value:
			break;
		case value_kind::array_value:
			break;
		}

		if (input.min.has_value()) {
			reject(input.min_span, std::string(subject) + " cannot use validation rule 'min' with type " +
			                           std::string(to_string(kind)));
		}
		if (input.max.has_value()) {
			reject(input.max_span, std::string(subject) + " cannot use validation rule 'max' with type " +
			                           std::string(to_string(kind)));
		}
		if (input.min_length.has_value()) {
			reject(input.min_length_span, std::string(subject) + " cannot use validation rule 'min_length' with type " +
			                                  std::string(to_string(kind)));
		}
		if (input.max_length.has_value()) {
			reject(input.max_length_span, std::string(subject) + " cannot use validation rule 'max_length' with type " +
			                                  std::string(to_string(kind)));
		}
		return output;
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
		static_cast<void>(
		    normalize_validation_rules(input.validation, input.kind, input.span, validation_subject("schema", hint)));

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
			const auto field_validation = normalize_validation_rules(
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
			    .validation = normalize_validation_rules(parameter.validation, parameter.kind, parameter.span,
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

	[[nodiscard]] std::optional<query_route_model>
	build_query_route_model(const request_model &request, const std::string &spec_name, source_span span) {
		query_route_model query_route;
		query_route.span = span;
		query_route.spec_name = spec_name;
		bool has_required_constraint = false;

		for (const auto &parameter : request.parameters) {
			if (parameter.location != parameter_location::query) {
				continue;
			}
			query_route.constraints.push_back(warp::http::compiled_query_constraint {
			    .name = parameter.source_name,
			    .presence = parameter.required ? warp::http::query_constraint_presence::required
			                                   : warp::http::query_constraint_presence::optional,
			});
			has_required_constraint = has_required_constraint || parameter.required;
		}

		if (!has_required_constraint) {
			return std::nullopt;
		}
		warp::http::detail::sort_compiled_query_constraints(query_route.constraints);
		return query_route;
	}

	void validate_route_group(resource_model &resource, route_group_model &group) {
		group.query_route_endpoint_indices.clear();
		group.fallback_endpoint_index.reset();
		group.routing_query_parameters.clear();

		for (const auto endpoint_index : group.endpoint_indices) {
			const auto &endpoint = resource.endpoints.at(endpoint_index);
			if (endpoint.query_route.has_value()) {
				group.query_route_endpoint_indices.push_back(endpoint_index);
				for (const auto constraint : endpoint.query_route->constraints) {
					append_unique(group.routing_query_parameters, constraint.name);
				}
				continue;
			}

			if (group.fallback_endpoint_index.has_value()) {
				fail(endpoint.span, "model.duplicate_route",
				     "duplicate route '" + group.path + "' for method " + std::string(to_string(group.method)) +
				         " requires deterministic query constraints or a single fallback endpoint");
			}
			group.fallback_endpoint_index = endpoint_index;
		}

		if (group.endpoint_indices.size() == 1 && group.query_route_endpoint_indices.size() == 1) {
			// A singleton endpoint does not need route-level query gating. Keeping it unconstrained
			// preserves binder-driven 400s for missing required query parameters.
			resource.endpoints.at(group.query_route_endpoint_indices.front()).query_route.reset();
			group.query_route_endpoint_indices.clear();
			group.routing_query_parameters.clear();
			return;
		}

		if (group.query_route_endpoint_indices.empty()) {
			if (group.endpoint_indices.size() > 1) {
				fail(resource.endpoints.at(group.endpoint_indices.back()).span, "model.duplicate_route",
				     "duplicate route '" + group.path + "' for method " + std::string(to_string(group.method)) +
				         " is ambiguous without required query parameter constraints");
			}
			return;
		}

		for (std::size_t i = 0; i < group.query_route_endpoint_indices.size(); ++i) {
			const auto left_index = group.query_route_endpoint_indices[i];
			const auto &left_endpoint = resource.endpoints.at(left_index);
			const auto left_constraints = effective_query_route_constraints(*left_endpoint.query_route, group);
			for (std::size_t j = i + 1; j < group.query_route_endpoint_indices.size(); ++j) {
				const auto right_index = group.query_route_endpoint_indices[j];
				const auto &right_endpoint = resource.endpoints.at(right_index);
				const auto right_constraints = effective_query_route_constraints(*right_endpoint.query_route, group);
				if (query_route_specs_overlap(left_constraints, right_constraints)) {
					fail(right_endpoint.span, "model.ambiguous_query_route",
					     "query-aware routes '" + left_endpoint.endpoint_name + "' and '" +
					         right_endpoint.endpoint_name + "' for " + std::string(to_string(group.method)) + " " +
					         group.path + " accept overlapping query parameter sets");
				}
			}
		}
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
			endpoint_model.query_route = build_query_route_model(
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

		for (auto &group : resource_model.route_groups) {
			validate_route_group(resource_model, group);
		}

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
