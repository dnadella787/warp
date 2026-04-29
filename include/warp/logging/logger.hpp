#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace warp::log {

enum class level {
	trace,
	debug,
	info,
	warn,
	error,
	critical,
	off
};

class logger {
public:
	explicit logger(std::shared_ptr<spdlog::logger> impl) noexcept;

	[[nodiscard]] static logger default_logger();
	[[nodiscard]] static logger stderr_color(std::string name);
	[[nodiscard]] static logger stdout_color(std::string name);

	[[nodiscard]] std::shared_ptr<spdlog::logger> native_handle() const noexcept;
	[[nodiscard]] std::string_view name() const noexcept;
	[[nodiscard]] explicit operator bool() const noexcept;

	void set_level(level new_level) const;
	[[nodiscard]] level current_level() const noexcept;
	void flush_on(level flush_level) const;
	void set_pattern(std::string_view pattern) const;
	void set_as_default() const;

	void trace(std::string_view message) const;
	void debug(std::string_view message) const;
	void info(std::string_view message) const;
	void warn(std::string_view message) const;
	void error(std::string_view message) const;
	void critical(std::string_view message) const;

	template <typename... Args>
	void trace(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		impl_->trace(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		impl_->debug(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void info(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		impl_->info(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		impl_->warn(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void error(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		impl_->error(fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		impl_->critical(fmt, std::forward<Args>(args)...);
	}

private:
	std::shared_ptr<spdlog::logger> impl_;
};

[[nodiscard]] logger default_logger();
void set_default_logger(const logger &new_default);
void set_level(level new_level);
void flush_on(level flush_level);
void set_pattern(std::string_view pattern);

void trace(std::string_view message);
void debug(std::string_view message);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);
void critical(std::string_view message);

template <typename... Args>
void trace(spdlog::format_string_t<Args...> fmt, Args &&...args) {
	spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
	spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
	spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
	spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
	spdlog::error(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
	spdlog::critical(fmt, std::forward<Args>(args)...);
}

} // namespace warp::log
