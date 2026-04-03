#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#include "warp/codegen/diagnostics.hpp"
#include "warp/codegen/spec_model.hpp"

namespace warp::codegen {

class spec_error : public diagnostic_error {
public:
	spec_error(source_span span, std::string code, std::string message);
	spec_error(std::size_t line, std::size_t column, std::string message);

	[[nodiscard]] std::size_t line() const noexcept;
	[[nodiscard]] std::size_t column() const noexcept;
};

[[nodiscard]] spec_ast parse_spec_ast(std::string_view yaml_text);
[[nodiscard]] spec_ast load_spec_ast(const std::filesystem::path &yaml_path);
[[nodiscard]] api_spec parse_api_spec(std::string_view yaml_text);
[[nodiscard]] api_spec load_api_spec(const std::filesystem::path &yaml_path);

} // namespace warp::codegen
