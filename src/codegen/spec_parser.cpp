#include "warp/codegen/spec_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace warp::codegen {

namespace {

struct yaml_node {
	enum class kind {
		scalar,
		map,
		list,
	};

	kind type {kind::scalar};
	std::string scalar;
	std::vector<std::pair<std::string, yaml_node>> map_values;
	std::vector<yaml_node> list_values;
	std::size_t line {0};
	std::size_t column {0};
};

struct source_line {
	std::size_t line {0};
	std::size_t indent {0};
	std::string text;
};

bool is_blank(std::string_view text) {
	return std::all_of(text.begin(), text.end(),
	                   [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
}

std::string trim(std::string_view text) {
	std::size_t begin = 0;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
		++begin;
	}
	std::size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
		--end;
	}
	return std::string(text.substr(begin, end - begin));
}

std::string strip_comment(std::string_view text) {
	bool single_quoted = false;
	bool double_quoted = false;
	for (std::size_t index = 0; index < text.size(); ++index) {
		const char c = text[index];
		if (c == '\'' && !double_quoted) {
			single_quoted = !single_quoted;
			continue;
		}
		if (c == '"' && !single_quoted) {
			double_quoted = !double_quoted;
			continue;
		}
		if (c == '#' && !single_quoted && !double_quoted) {
			return std::string(text.substr(0, index));
		}
	}
	return std::string(text);
}

std::vector<source_line> tokenize_lines(std::string_view yaml_text) {
	std::vector<source_line> lines;
	std::size_t line_number = 1;
	std::size_t start = 0;
	while (start <= yaml_text.size()) {
		const auto end = yaml_text.find('\n', start);
		auto raw = yaml_text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (!raw.empty() && raw.back() == '\r') {
			raw.remove_suffix(1);
		}

		const auto uncommented = strip_comment(raw);
		if (!is_blank(uncommented)) {
			std::size_t indent = 0;
			while (indent < uncommented.size() && uncommented[indent] == ' ') {
				++indent;
			}
			if (indent < uncommented.size() && uncommented[indent] == '\t') {
				throw spec_error(line_number, indent + 1, "tabs are not supported in YAML indentation");
			}
			lines.push_back(source_line {
			    .line = line_number,
			    .indent = indent,
			    .text = std::string(uncommented.substr(indent)),
			});
		}

		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
		++line_number;
	}
	return lines;
}

std::size_t find_mapping_separator(std::string_view text) {
	bool single_quoted = false;
	bool double_quoted = false;
	for (std::size_t index = 0; index < text.size(); ++index) {
		const char c = text[index];
		if (c == '\'' && !double_quoted) {
			single_quoted = !single_quoted;
		} else if (c == '"' && !single_quoted) {
			double_quoted = !double_quoted;
		} else if (c == ':' && !single_quoted && !double_quoted) {
			return index;
		}
	}
	return std::string_view::npos;
}

std::string parse_scalar_text(std::string text) {
	text = trim(text);
	if (text.size() >= 2 &&
	    ((text.front() == '"' && text.back() == '"') || (text.front() == '\'' && text.back() == '\''))) {
		text = text.substr(1, text.size() - 2);
	}
	return text;
}

class yaml_parser {
public:
	explicit yaml_parser(std::vector<source_line> lines) : lines_(std::move(lines)) {
	}

	[[nodiscard]] yaml_node parse_document() {
		if (lines_.empty()) {
			return yaml_node {.type = yaml_node::kind::map, .line = 1, .column = 1};
		}
		std::size_t index = 0;
		auto document = parse_block(index, lines_[0].indent);
		if (index != lines_.size()) {
			throw spec_error(lines_[index].line, lines_[index].indent + 1, "unexpected trailing YAML content");
		}
		return document;
	}

private:
	[[nodiscard]] yaml_node parse_block(std::size_t &index, std::size_t indent) {
		if (index >= lines_.size()) {
			throw spec_error(lines_.back().line, lines_.back().indent + 1, "expected nested YAML block");
		}
		if (lines_[index].indent < indent) {
			throw spec_error(lines_[index].line, lines_[index].indent + 1, "invalid indentation");
		}
		if (lines_[index].text.rfind("- ", 0) == 0 || lines_[index].text == "-") {
			return parse_list(index, indent);
		}
		return parse_map(index, indent);
	}

	[[nodiscard]] yaml_node parse_map(std::size_t &index, std::size_t indent) {
		yaml_node node {.type = yaml_node::kind::map, .line = lines_[index].line, .column = indent + 1};
		while (index < lines_.size()) {
			const auto &line = lines_[index];
			if (line.indent < indent) {
				break;
			}
			if (line.indent > indent) {
				throw spec_error(line.line, line.indent + 1, "unexpected indentation inside mapping");
			}
			if (line.text.rfind("- ", 0) == 0 || line.text == "-") {
				throw spec_error(line.line, line.indent + 1, "list item cannot appear at mapping scope");
			}

			const auto separator = find_mapping_separator(line.text);
			if (separator == std::string_view::npos) {
				throw spec_error(line.line, line.indent + 1, "expected 'key: value' mapping entry");
			}

			const std::string key = trim(std::string_view(line.text).substr(0, separator));
			if (key.empty()) {
				throw spec_error(line.line, line.indent + 1, "mapping key cannot be empty");
			}

			const std::string remainder = trim(std::string_view(line.text).substr(separator + 1));
			++index;

			yaml_node value;
			if (!remainder.empty()) {
				value.type = yaml_node::kind::scalar;
				value.scalar = parse_scalar_text(remainder);
				value.line = line.line;
				value.column = line.indent + separator + 2;
			} else {
				if (index >= lines_.size() || lines_[index].indent <= indent) {
					throw spec_error(line.line, line.indent + separator + 1, "expected nested block after mapping key");
				}
				value = parse_block(index, lines_[index].indent);
			}
			node.map_values.emplace_back(key, std::move(value));
		}
		return node;
	}

	[[nodiscard]] yaml_node parse_list(std::size_t &index, std::size_t indent) {
		yaml_node node {.type = yaml_node::kind::list, .line = lines_[index].line, .column = indent + 1};
		while (index < lines_.size()) {
			const auto &line = lines_[index];
			if (line.indent < indent) {
				break;
			}
			if (line.indent > indent) {
				throw spec_error(line.line, line.indent + 1, "unexpected indentation inside list");
			}
			if (!(line.text.rfind("- ", 0) == 0 || line.text == "-")) {
				break;
			}

			const std::string remainder = line.text == "-" ? std::string {} : line.text.substr(2);
			++index;

			if (remainder.empty()) {
				if (index >= lines_.size() || lines_[index].indent <= indent) {
					throw spec_error(line.line, line.indent + 1, "expected nested block after list item");
				}
				node.list_values.push_back(parse_block(index, lines_[index].indent));
				continue;
			}

			const auto separator = find_mapping_separator(remainder);
			if (separator != std::string_view::npos) {
				yaml_node item {.type = yaml_node::kind::map, .line = line.line, .column = indent + 1};
				const std::string key = trim(std::string_view(remainder).substr(0, separator));
				if (key.empty()) {
					throw spec_error(line.line, line.indent + 3, "list item mapping key cannot be empty");
				}
				const std::string value_text = trim(std::string_view(remainder).substr(separator + 1));
				if (!value_text.empty()) {
					item.map_values.emplace_back(key, yaml_node {
					                                      .type = yaml_node::kind::scalar,
					                                      .scalar = parse_scalar_text(value_text),
					                                      .line = line.line,
					                                      .column = indent + separator + 4,
					                                  });
				} else {
					if (index >= lines_.size() || lines_[index].indent <= indent) {
						throw spec_error(line.line, line.indent + separator + 3,
						                 "expected nested block after mapping key");
					}
					item.map_values.emplace_back(key, parse_block(index, lines_[index].indent));
				}

				if (index < lines_.size() && lines_[index].indent > indent) {
					auto continuation = parse_map(index, lines_[index].indent);
					for (auto &entry : continuation.map_values) {
						item.map_values.push_back(std::move(entry));
					}
				}

				node.list_values.push_back(std::move(item));
				continue;
			}

			node.list_values.push_back(yaml_node {
			    .type = yaml_node::kind::scalar,
			    .scalar = parse_scalar_text(remainder),
			    .line = line.line,
			    .column = indent + 3,
			});
		}
		return node;
	}

	std::vector<source_line> lines_;
};

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
	schema parsed = schema::object();
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
		expect_kind(field_node, yaml_node::kind::map, "schema field");
		const auto field_name = parse_string(required_key(field_node, "name"), "field name");
		bool required = true;
		if (const auto *field_required = find_key(field_node, "required")) {
			required = parse_bool(*field_required, "field required");
		}
		if (const auto *field_schema = find_key(field_node, "schema")) {
			parsed.append_field(field_name, parse_schema(*field_schema), required);
		} else {
			parsed.append_field(field_name, parse_schema(field_node, false), required);
		}
	}
	return parsed;
}

schema parse_schema(const yaml_node &node, bool allow_schema_name) {
	if (node.type == yaml_node::kind::scalar) {
		switch (parse_scalar_kind(node, "schema type")) {
		case value_kind::string_value:
			return schema::string();
		case value_kind::int64_value:
			return schema::int64();
		case value_kind::double_value:
			return schema::number();
		case value_kind::bool_value:
			return schema::boolean();
		case value_kind::object_value:
		case value_kind::array_value:
			break;
		}
	}
	expect_kind(node, yaml_node::kind::map, "schema");
	const auto &type_node = required_key(node, "type");
	const auto type_name = parse_string(type_node, "schema type");

	if (type_name == "object") {
		return parse_object_schema(node, allow_schema_name);
	}
	if (type_name == "array") {
		schema parsed = schema::array(parse_schema(required_key(node, "items")));
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
	if (const auto *nullable = find_key(node, "nullable")) {
		parsed.nullable = parse_bool(*nullable, "schema nullable");
	}
	return parsed;
}

parameter_spec parse_parameter(const yaml_node &node) {
	expect_kind(node, yaml_node::kind::map, "parameter");
	parameter_spec parameter;
	parameter.name = parse_string(required_key(node, "name"), "parameter name");
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
	return parameter;
}

request_spec parse_request(const yaml_node &node) {
	request_spec request;
	if (node.type != yaml_node::kind::map ||
	    (find_key(node, "parameters") == nullptr && find_key(node, "body") == nullptr)) {
		request.body = parse_schema(node);
		return request;
	}
	expect_kind(node, yaml_node::kind::map, "request");
	if (const auto *parameters = find_key(node, "parameters")) {
		const auto &list = expect_kind(*parameters, yaml_node::kind::list, "request parameters");
		for (const auto &parameter_node : list.list_values) {
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
	if (node.type != yaml_node::kind::map ||
	    (find_key(node, "status") == nullptr && find_key(node, "body") == nullptr)) {
		response.body = parse_schema(node);
		return response;
	}
	expect_kind(node, yaml_node::kind::map, "response");
	if (const auto *status = find_key(node, "status")) {
		response.status_code = parse_int(*status, "response status");
	}
	if (response.status_code < 100 || response.status_code > 599) {
		throw spec_error(node.line, node.column, "response status must be between 100 and 599");
	}
	if (const auto *body = find_key(node, "body")) {
		response.body = parse_schema(*body);
	}
	return response;
}

endpoint_spec parse_endpoint(const yaml_node &node) {
	expect_kind(node, yaml_node::kind::map, "endpoint");
	endpoint_spec endpoint;
	endpoint.method = find_key(node, "method") == nullptr ? http_method::get : parse_method(*find_key(node, "method"));
	endpoint.path = parse_string(required_key(node, "path"), "endpoint path");
	endpoint.name = find_key(node, "name") == nullptr ? derive_endpoint_name(endpoint.method, endpoint.path)
	                                                  : parse_string(*find_key(node, "name"), "endpoint name");
	if (const auto *request = find_key(node, "request")) {
		endpoint.request = parse_request(*request);
	}
	if (const auto *response = find_key(node, "response")) {
		endpoint.response = parse_response(*response);
	}
	return endpoint;
}

resource_spec parse_resource(const yaml_node &node) {
	expect_kind(node, yaml_node::kind::map, "resource");
	resource_spec resource;
	resource.name = parse_string(required_key(node, "name"), "resource name");
	const auto &endpoints = expect_kind(required_key(node, "endpoints"), yaml_node::kind::list, "resource endpoints");
	for (const auto &endpoint_node : endpoints.list_values) {
		resource.endpoints.push_back(parse_endpoint(endpoint_node));
	}
	return resource;
}

std::string read_file(const std::filesystem::path &path) {
	std::ifstream input(path);
	if (!input.is_open()) {
		throw std::runtime_error("failed to open YAML spec: " + path.string());
	}
	std::ostringstream buffer;
	buffer << input.rdbuf();
	return buffer.str();
}

} // namespace

spec_error::spec_error(std::size_t line, std::size_t column, std::string message)
    : std::runtime_error("line " + std::to_string(line) + ", column " + std::to_string(column) + ": " +
                         std::move(message)),
      line_(line), column_(column) {
}

std::size_t spec_error::line() const noexcept {
	return line_;
}

std::size_t spec_error::column() const noexcept {
	return column_;
}

api_spec parse_api_spec(std::string_view yaml_text) {
	yaml_parser parser(tokenize_lines(yaml_text));
	const auto document = parser.parse_document();
	api_spec spec;

	const auto &root = expect_kind(document, yaml_node::kind::map, "root YAML document");
	if (const auto *namespace_node = find_key(root, "namespace")) {
		spec.cpp_namespace = parse_string(*namespace_node, "namespace");
	} else if (const auto *namespace_node = find_key(root, "cpp_namespace")) {
		spec.cpp_namespace = parse_string(*namespace_node, "cpp namespace");
	}
	if (const auto *resources = find_key(root, "resources")) {
		const auto &list = expect_kind(*resources, yaml_node::kind::list, "resources");
		for (const auto &resource_node : list.list_values) {
			spec.resources.push_back(parse_resource(resource_node));
		}
		return spec;
	}

	if (const auto *endpoints = find_key(root, "endpoints")) {
		resource_spec resource;
		resource.name =
		    find_key(root, "name") == nullptr ? "default" : parse_string(*find_key(root, "name"), "resource name");
		const auto &list = expect_kind(*endpoints, yaml_node::kind::list, "endpoints");
		for (const auto &endpoint_node : list.list_values) {
			resource.endpoints.push_back(parse_endpoint(endpoint_node));
		}
		spec.resources.push_back(std::move(resource));
		return spec;
	}

	throw spec_error(root.line, root.column, "root YAML document must contain 'resources' or 'endpoints'");
}

api_spec load_api_spec(const std::filesystem::path &yaml_path) {
	return parse_api_spec(read_file(yaml_path));
}

} // namespace warp::codegen
