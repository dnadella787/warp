#include "codegen/spec_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "codegen/spec_ast_decoder.hpp"
#include "codegen/yaml_document_parser.hpp"

namespace warp::codegen {

namespace {

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

spec_error::spec_error(source_span span, std::string code, std::string message)
    : diagnostic_error(diagnostic {.severity = diagnostic_severity::error,
                                   .code = std::move(code),
                                   .message = std::move(message),
                                   .span = span}) {
}

spec_error::spec_error(std::size_t line, std::size_t column, std::string message)
    : spec_error(source_span {.line = line, .column = column}, "spec.parse", std::move(message)) {
}

std::size_t spec_error::line() const noexcept {
	return item().span.line;
}

std::size_t spec_error::column() const noexcept {
	return item().span.column;
}

spec_ast parse_spec_ast(std::string_view yaml_text) {
	return detail::decode_spec_ast(detail::parse_yaml_document(yaml_text));
}

spec_ast load_spec_ast(const std::filesystem::path &yaml_path) {
	return parse_spec_ast(read_file(yaml_path));
}

api_spec parse_api_spec(std::string_view yaml_text) {
	return parse_spec_ast(yaml_text);
}

api_spec load_api_spec(const std::filesystem::path &yaml_path) {
	return load_spec_ast(yaml_path);
}

} // namespace warp::codegen
