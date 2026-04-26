#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "codegen/diagnostics.hpp"

namespace warp::codegen {

enum class value_kind {
	string_value,
	int64_value,
	double_value,
	bool_value,
	object_value,
	array_value,
};

[[nodiscard]] std::string_view to_string(value_kind kind) noexcept;
[[nodiscard]] bool is_primitive(value_kind kind) noexcept;

struct schema;

struct schema_field {
	source_span span {};
	std::string name;
	schema *value {nullptr};
	bool required {true};
};

struct schema {
	source_span span {};
	value_kind kind {value_kind::object_value};
	bool nullable {false};
	std::string name;
	std::vector<std::unique_ptr<schema>> owned_children;
	std::vector<schema_field> fields;
	schema *element_type {nullptr};

	schema() = default;
	explicit schema(value_kind value) : kind(value) {
	}

	schema(const schema &other);
	schema &operator=(const schema &other);
	schema(schema &&) noexcept = default;
	schema &operator=(schema &&) noexcept = default;

	[[nodiscard]] static schema string(bool nullable = false);
	[[nodiscard]] static schema int64(bool nullable = false);
	[[nodiscard]] static schema number(bool nullable = false);
	[[nodiscard]] static schema boolean(bool nullable = false);
	[[nodiscard]] static schema object(std::string name = {}, bool nullable = false);
	[[nodiscard]] static schema array(schema element_type, std::string name = {}, bool nullable = false);

	schema &append_field(source_span field_span, std::string field_name, schema field_schema, bool required = true);
	schema &append_field(std::string field_name, schema field_schema, bool required = true);
	schema &set_element(source_span element_span, schema element_schema);
	schema &set_element(schema element_schema);
};

enum class parameter_location {
	path,
	query,
	header,
};

[[nodiscard]] std::string_view to_string(parameter_location location) noexcept;

enum class http_method {
	get,
	post,
	put,
	patch,
	delete_,
};

[[nodiscard]] std::string_view to_string(http_method method) noexcept;

struct parameter_spec {
	source_span span {};
	std::string name;
	parameter_location location {parameter_location::query};
	value_kind kind {value_kind::string_value};
	bool required {false};
};

struct request_spec {
	source_span span {};
	std::vector<parameter_spec> parameters;
	std::optional<schema> body;
};

struct response_spec {
	source_span span {};
	source_span status_span {};
	int status_code {200};
	std::optional<schema> body;
};

struct endpoint_spec {
	source_span span {};
	source_span name_span {};
	source_span method_span {};
	source_span path_span {};
	std::string name;
	http_method method {http_method::get};
	std::string path;
	request_spec request;
	response_spec response;
};

struct resource_spec {
	source_span span {};
	source_span name_span {};
	std::string name;
	std::vector<endpoint_spec> endpoints;
};

struct api_spec {
	source_span span {};
	source_span namespace_span {};
	std::string cpp_namespace {"generated"};
	std::vector<resource_spec> resources;
};

using field_ast = schema_field;
using schema_ast = schema;
using parameter_ast = parameter_spec;
using request_ast = request_spec;
using response_ast = response_spec;
using endpoint_ast = endpoint_spec;
using resource_ast = resource_spec;
using spec_ast = api_spec;

} // namespace warp::codegen
