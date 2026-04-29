#pragma once

#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "warp/logging/sink.hpp"

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

namespace detail {

[[nodiscard]] spdlog::level::level_enum to_spdlog_level(level value) noexcept;

} // namespace detail

class logger {
public:
	template <std::input_iterator It>
	logger(std::string name, It begin, It end) : impl_(make_impl(std::move(name), std::vector<sink>(begin, end))) {
	}

	logger(std::string name, sink single_sink);
	logger(std::string name, std::initializer_list<sink> sinks);
	~logger();
	logger(const logger &);
	logger(logger &&) noexcept;
	logger &operator=(const logger &);
	logger &operator=(logger &&) noexcept;

	[[nodiscard]] static logger default_logger();
	[[nodiscard]] static logger stderr_color(std::string name);
	[[nodiscard]] static logger stdout_color(std::string name);

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
		log_formatted(level::trace, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		log_formatted(level::debug, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void info(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		log_formatted(level::info, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		log_formatted(level::warn, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void error(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		log_formatted(level::error, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		log_formatted(level::critical, fmt, std::forward<Args>(args)...);
	}

private:
	friend void set_default_logger(const logger &new_default);
	explicit logger(std::shared_ptr<spdlog::logger> impl) noexcept;
	[[nodiscard]] static std::shared_ptr<spdlog::logger> make_impl(std::string name, sink single_sink);
	[[nodiscard]] static std::shared_ptr<spdlog::logger> make_impl(std::string name, std::initializer_list<sink> sinks);
	[[nodiscard]] static std::shared_ptr<spdlog::logger> make_impl(std::string name, std::vector<sink> sinks);
	void log_message(level log_level, std::string_view message) const;

	template <typename... Args>
	void log_formatted(level log_level, spdlog::format_string_t<Args...> fmt, Args &&...args) const {
		if (!impl_) {
			return;
		}
		impl_->log(detail::to_spdlog_level(log_level), fmt, std::forward<Args>(args)...);
	}

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
