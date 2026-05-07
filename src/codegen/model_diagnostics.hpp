#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "codegen/diagnostics.hpp"

namespace warp::codegen::detail {

[[noreturn]] inline void fail(source_span span, std::string code, std::string message) {
	throw diagnostic_error(diagnostic {
	    .severity = diagnostic_severity::error, .code = std::move(code), .message = std::move(message), .span = span});
}

[[nodiscard]] inline source_span validation_span_or(source_span span, source_span fallback) {
	return span.line == 0 ? fallback : span;
}

[[nodiscard]] inline std::string validation_subject(std::string_view noun, std::string_view name) {
	return std::string(noun) + " '" + std::string(name) + "'";
}

} // namespace warp::codegen::detail
