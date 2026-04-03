#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace warp::codegen {

struct source_span {
	std::size_t line {0};
	std::size_t column {0};
};

enum class diagnostic_severity {
	error,
};

struct diagnostic {
	diagnostic_severity severity {diagnostic_severity::error};
	std::string code;
	std::string message;
	source_span span {};
};

inline std::string format_diagnostic(const diagnostic &item) {
	std::string formatted =
	    "line " + std::to_string(item.span.line) + ", column " + std::to_string(item.span.column) + ": ";
	if (!item.code.empty()) {
		formatted += "[" + item.code + "] ";
	}
	formatted += item.message;
	return formatted;
}

class diagnostic_error : public std::runtime_error {
public:
	explicit diagnostic_error(diagnostic item) : std::runtime_error(format_diagnostic(item)), item_(std::move(item)) {
	}

	[[nodiscard]] const diagnostic &item() const noexcept {
		return item_;
	}

private:
	diagnostic item_;
};

class diagnostic_sink {
public:
	void error(std::string code, source_span span, std::string message) {
		items_.push_back(diagnostic {.severity = diagnostic_severity::error,
		                             .code = std::move(code),
		                             .message = std::move(message),
		                             .span = span});
	}

	[[nodiscard]] bool has_errors() const noexcept {
		return !items_.empty();
	}

	[[nodiscard]] const std::vector<diagnostic> &items() const noexcept {
		return items_;
	}

private:
	std::vector<diagnostic> items_;
};

} // namespace warp::codegen
