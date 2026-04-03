#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "warp/codegen/spec_model.hpp"

namespace warp::codegen {

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
	std::string json_name;
	std::string member_name;
	schema_type type;
	bool required {true};
};

struct object_schema_model {
	std::string name;
	std::vector<field_model> fields;
};

struct parameter_model {
	std::string source_name;
	std::string member_name;
	parameter_location location {parameter_location::query};
	schema_type type;
	bool required {true};
};

struct request_model {
	std::string name;
	std::optional<std::string> body_type_name;
	std::vector<parameter_model> parameters;
};

struct response_model {
	int status_code {200};
	std::optional<std::string> body_type_name;
};

struct endpoint_model {
	std::string resource_name;
	std::string endpoint_name;
	std::string request_name;
	std::string result_name;
	std::string handler_name;
	http_method method {http_method::get};
	std::string path;
	request_model request;
	response_model response;
};

struct resource_model {
	std::string name;
	std::string class_name;
	std::vector<endpoint_model> endpoints;
};

struct api_model {
	std::vector<object_schema_model> schemas;
	std::vector<resource_model> resources;
};

[[nodiscard]] api_model build_api_model(const api_spec &spec);

} // namespace warp::codegen
