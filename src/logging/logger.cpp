#include "warp/logging/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace warp::log {
namespace {

[[nodiscard]] spdlog::level::level_enum to_spdlog_level(level value) noexcept {
	switch (value) {
		case level::trace:
			return spdlog::level::trace;
		case level::debug:
			return spdlog::level::debug;
		case level::info:
			return spdlog::level::info;
		case level::warn:
			return spdlog::level::warn;
		case level::error:
			return spdlog::level::err;
		case level::critical:
			return spdlog::level::critical;
		case level::off:
			return spdlog::level::off;
	}
	return spdlog::level::off;
}

[[nodiscard]] level from_spdlog_level(spdlog::level::level_enum value) noexcept {
	switch (value) {
		case spdlog::level::trace:
			return level::trace;
		case spdlog::level::debug:
			return level::debug;
		case spdlog::level::info:
			return level::info;
		case spdlog::level::warn:
			return level::warn;
		case spdlog::level::err:
			return level::error;
		case spdlog::level::critical:
			return level::critical;
		case spdlog::level::off:
		case spdlog::level::n_levels:
			return level::off;
	}
	return level::off;
}

} // namespace

logger::logger(std::shared_ptr<spdlog::logger> impl) noexcept : impl_(std::move(impl)) {
}

logger logger::default_logger() {
	return logger(spdlog::default_logger());
}

logger logger::stderr_color(std::string name) {
	return logger(spdlog::stderr_color_mt(std::move(name)));
}

logger logger::stdout_color(std::string name) {
	return logger(spdlog::stdout_color_mt(std::move(name)));
}

std::shared_ptr<spdlog::logger> logger::native_handle() const noexcept {
	return impl_;
}

std::string_view logger::name() const noexcept {
	if (!impl_) {
		return {};
	}
	return impl_->name();
}

logger::operator bool() const noexcept {
	return static_cast<bool>(impl_);
}

void logger::trace(std::string_view message) const {
	impl_->trace(message);
}

void logger::debug(std::string_view message) const {
	impl_->debug(message);
}

void logger::info(std::string_view message) const {
	impl_->info(message);
}

void logger::warn(std::string_view message) const {
	impl_->warn(message);
}

void logger::error(std::string_view message) const {
	impl_->error(message);
}

void logger::critical(std::string_view message) const {
	impl_->critical(message);
}

void logger::set_level(level new_level) const {
	impl_->set_level(to_spdlog_level(new_level));
}

level logger::current_level() const noexcept {
	if (!impl_) {
		return level::off;
	}
	return from_spdlog_level(impl_->level());
}

void logger::flush_on(level flush_level) const {
	impl_->flush_on(to_spdlog_level(flush_level));
}

void logger::set_pattern(std::string_view pattern) const {
	impl_->set_pattern(std::string(pattern));
}

void logger::set_as_default() const {
	spdlog::set_default_logger(impl_);
}

logger default_logger() {
	return logger::default_logger();
}

void set_default_logger(const logger &new_default) {
	spdlog::set_default_logger(new_default.native_handle());
}

void set_level(level new_level) {
	spdlog::set_level(to_spdlog_level(new_level));
}

void flush_on(level flush_level) {
	spdlog::flush_on(to_spdlog_level(flush_level));
}

void set_pattern(std::string_view pattern) {
	spdlog::set_pattern(std::string(pattern));
}

void trace(std::string_view message) {
	spdlog::trace(message);
}

void debug(std::string_view message) {
	spdlog::debug(message);
}

void info(std::string_view message) {
	spdlog::info(message);
}

void warn(std::string_view message) {
	spdlog::warn(message);
}

void error(std::string_view message) {
	spdlog::error(message);
}

void critical(std::string_view message) {
	spdlog::critical(message);
}

} // namespace warp::logging
