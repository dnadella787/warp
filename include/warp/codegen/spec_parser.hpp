#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#include "warp/codegen/spec_model.hpp"

namespace warp::codegen {

class spec_error : public std::runtime_error {
public:
	spec_error(std::size_t line, std::size_t column, std::string message);

	[[nodiscard]] std::size_t line() const noexcept;
	[[nodiscard]] std::size_t column() const noexcept;

private:
	std::size_t line_ {0};
	std::size_t column_ {0};
};

[[nodiscard]] api_spec parse_api_spec(std::string_view yaml_text);
[[nodiscard]] api_spec load_api_spec(const std::filesystem::path &yaml_path);

} // namespace warp::codegen
