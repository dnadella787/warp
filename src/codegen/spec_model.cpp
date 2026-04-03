#include "warp/codegen/spec_model.hpp"

#include <utility>

namespace warp::codegen {

namespace {

schema clone_schema(const schema &source) {
	schema copy(source.kind);
	copy.span = source.span;
	copy.nullable = source.nullable;
	copy.name = source.name;

	for (const auto &child : source.owned_children) {
		auto child_copy = std::make_unique<schema>(clone_schema(*child));
		schema *child_ptr = child_copy.get();
		copy.owned_children.push_back(std::move(child_copy));

		if (source.element_type == child.get()) {
			copy.element_type = child_ptr;
		}
	}

	for (const auto &field : source.fields) {
		schema_field field_copy;
		field_copy.span = field.span;
		field_copy.name = field.name;
		field_copy.required = field.required;
		for (std::size_t i = 0; i < source.owned_children.size(); ++i) {
			if (source.owned_children[i].get() == field.value) {
				field_copy.value = copy.owned_children[i].get();
				break;
			}
		}
		copy.fields.push_back(std::move(field_copy));
	}

	return copy;
}

schema primitive(value_kind kind, bool nullable) {
	schema out(kind);
	out.nullable = nullable;
	return out;
}

} // namespace

std::string_view to_string(value_kind kind) noexcept {
	switch (kind) {
	case value_kind::string_value:
		return "string";
	case value_kind::int64_value:
		return "int64";
	case value_kind::double_value:
		return "double";
	case value_kind::bool_value:
		return "bool";
	case value_kind::object_value:
		return "object";
	case value_kind::array_value:
		return "array";
	}
	return "unknown";
}

bool is_primitive(value_kind kind) noexcept {
	return kind == value_kind::string_value || kind == value_kind::int64_value || kind == value_kind::double_value ||
	       kind == value_kind::bool_value;
}

schema::schema(const schema &other) : schema(clone_schema(other)) {
}

schema &schema::operator=(const schema &other) {
	if (this == &other) {
		return *this;
	}
	*this = clone_schema(other);
	return *this;
}

schema schema::string(bool nullable) {
	return primitive(value_kind::string_value, nullable);
}

schema schema::int64(bool nullable) {
	return primitive(value_kind::int64_value, nullable);
}

schema schema::number(bool nullable) {
	return primitive(value_kind::double_value, nullable);
}

schema schema::boolean(bool nullable) {
	return primitive(value_kind::bool_value, nullable);
}

schema schema::object(std::string name, bool nullable) {
	schema out(value_kind::object_value);
	out.nullable = nullable;
	out.name = std::move(name);
	return out;
}

schema schema::array(schema element_type, std::string name, bool nullable) {
	schema out(value_kind::array_value);
	out.nullable = nullable;
	out.name = std::move(name);
	out.set_element(std::move(element_type));
	return out;
}

schema &schema::append_field(std::string field_name, schema field_schema, bool required) {
	return append_field({}, std::move(field_name), std::move(field_schema), required);
}

schema &schema::append_field(source_span field_span, std::string field_name, schema field_schema, bool required) {
	auto child = std::make_unique<schema>(std::move(field_schema));
	schema *field_ptr = child.get();
	owned_children.push_back(std::move(child));
	fields.push_back(
	    schema_field {.span = field_span, .name = std::move(field_name), .value = field_ptr, .required = required});
	return *this;
}

schema &schema::set_element(schema element_schema) {
	return set_element({}, std::move(element_schema));
}

schema &schema::set_element(source_span element_span, schema element_schema) {
	auto child = std::make_unique<schema>(std::move(element_schema));
	child->span = element_span;
	element_type = child.get();
	owned_children.push_back(std::move(child));
	return *this;
}

std::string_view to_string(parameter_location location) noexcept {
	switch (location) {
	case parameter_location::path:
		return "path";
	case parameter_location::query:
		return "query";
	case parameter_location::header:
		return "header";
	}
	return "unknown";
}

std::string_view to_string(http_method method) noexcept {
	switch (method) {
	case http_method::get:
		return "GET";
	case http_method::post:
		return "POST";
	case http_method::put:
		return "PUT";
	case http_method::patch:
		return "PATCH";
	case http_method::delete_:
		return "DELETE";
	}
	return "UNKNOWN";
}

} // namespace warp::codegen
