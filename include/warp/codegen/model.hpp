#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "warp/codegen/spec_model.hpp"
#include "warp/http/query_constraint_semantics.hpp"
#include "warp/http/route_pattern.hpp"

namespace warp::codegen {

enum class http_body_mode {
	forbidden,
	optional,
	required,
};

struct schema_type {
	enum class kind {
		string_value,
		int64_value,
		double_value,
		bool_value,
		object_value,
		array_value,
	};

	kind type {kind::string_value};
	std::string object_name;
	std::unique_ptr<schema_type> element_type;

	schema_type() = default;
	explicit schema_type(kind value) : type(value) {
	}
	schema_type(const schema_type &other);
	schema_type &operator=(const schema_type &other);
	schema_type(schema_type &&) noexcept = default;
	schema_type &operator=(schema_type &&) noexcept = default;
};

struct field_model {
	source_span span {};
	std::string json_name;
	std::string member_name;
	schema_type type;
	bool required {true};
};

struct object_schema_model {
	source_span span {};
	std::string name;
	std::vector<field_model> fields;
};

struct parameter_model {
	source_span span {};
	std::string source_name;
	std::string member_name;
	parameter_location location {parameter_location::query};
	schema_type type;
	bool required {true};
};

struct request_model {
	source_span span {};
	std::string name;
	std::optional<std::string> body_type_name;
	std::vector<parameter_model> parameters;
	http_body_mode body_mode {http_body_mode::forbidden};
};

struct response_model {
	source_span span {};
	source_span status_span {};
	int status_code {200};
	std::optional<std::string> body_type_name;
	http_body_mode body_mode {http_body_mode::forbidden};
};

struct query_route_model {
	source_span span {};
	std::string spec_name;
	std::vector<warp::http::compiled_query_constraint> constraints;
};

struct endpoint_model {
	source_span span {};
	std::string resource_name;
	std::string endpoint_name;
	std::string request_name;
	std::string result_name;
	std::string handler_name;
	http_method method {http_method::get};
	std::string path;
	warp::http::route_pattern route;
	request_model request;
	response_model response;
	std::optional<query_route_model> query_route;
};

struct route_group_model {
	http_method method {http_method::get};
	std::string path;
	std::vector<std::size_t> endpoint_indices;
	std::vector<std::size_t> query_route_endpoint_indices;
	std::optional<std::size_t> fallback_endpoint_index;
	std::vector<std::string> routing_query_parameters;
};

struct resource_model {
	source_span span {};
	std::string name;
	std::string routes_class_name;
	std::vector<endpoint_model> endpoints;
	std::vector<route_group_model> route_groups;
};

struct api_model {
	std::string cpp_namespace;
	std::vector<object_schema_model> schemas;
	std::vector<resource_model> resources;
};

using ApiModel = api_model;

[[nodiscard]] api_model build_api_model(const spec_ast &spec, std::string_view namespace_override = {});

} // namespace warp::codegen
