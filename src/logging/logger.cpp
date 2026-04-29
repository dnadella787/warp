#include "warp/logging/logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace warp::log {
namespace detail {

spdlog::level::level_enum to_spdlog_level(level value) noexcept {
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

} // namespace detail

namespace {

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

void log_with_spdlog_logger(const std::shared_ptr<spdlog::logger> &native_logger, level log_level,
                            std::string_view message) {
	if (!native_logger) {
		return;
	}
	native_logger->log(detail::to_spdlog_level(log_level), message);
}

} // namespace

struct sink::impl {
	explicit impl(std::shared_ptr<spdlog::sinks::sink> native) noexcept : native_(std::move(native)) {
	}

	std::shared_ptr<spdlog::sinks::sink> native_;
};

sink::sink(std::shared_ptr<impl> impl) noexcept : impl_(std::move(impl)) {
}

sink::~sink() = default;
sink::sink(const sink &) = default;
sink::sink(sink &&) noexcept = default;
sink &sink::operator=(const sink &) = default;
sink &sink::operator=(sink &&) noexcept = default;

sink sink::stderr_color() {
	return sink(std::make_shared<impl>(std::make_shared<spdlog::sinks::stderr_color_sink_mt>()));
}

sink sink::stdout_color() {
	return sink(std::make_shared<impl>(std::make_shared<spdlog::sinks::stdout_color_sink_mt>()));
}

sink sink::basic_file(std::string filename, bool truncate) {
	return sink(
	    std::make_shared<impl>(std::make_shared<spdlog::sinks::basic_file_sink_mt>(std::move(filename), truncate)));
}

sink::operator bool() const noexcept {
	return static_cast<bool>(impl_) && static_cast<bool>(impl_->native_);
}

logger::logger(std::string name, sink single_sink) : impl_(make_impl(std::move(name), std::move(single_sink))) {
}

logger::logger(std::string name, std::initializer_list<sink> sinks) : impl_(make_impl(std::move(name), sinks)) {
}

logger::logger(std::shared_ptr<spdlog::logger> impl) noexcept : impl_(std::move(impl)) {
}

logger::~logger() = default;
logger::logger(const logger &) = default;
logger::logger(logger &&) noexcept = default;
logger &logger::operator=(const logger &) = default;
logger &logger::operator=(logger &&) noexcept = default;

std::shared_ptr<spdlog::logger> logger::make_impl(std::string name, sink single_sink) {
	std::vector<spdlog::sink_ptr> native_sinks;
	if (single_sink.impl_ && single_sink.impl_->native_) {
		native_sinks.push_back(std::move(single_sink.impl_->native_));
	}
	return std::make_shared<spdlog::logger>(std::move(name), native_sinks.begin(), native_sinks.end());
}

std::shared_ptr<spdlog::logger> logger::make_impl(std::string name, std::initializer_list<sink> sinks) {
	std::vector<spdlog::sink_ptr> native_sinks;
	native_sinks.reserve(sinks.size());
	for (const auto &sink : sinks) {
		if (sink.impl_ && sink.impl_->native_) {
			native_sinks.push_back(sink.impl_->native_);
		}
	}
	return std::make_shared<spdlog::logger>(std::move(name), native_sinks.begin(), native_sinks.end());
}

std::shared_ptr<spdlog::logger> logger::make_impl(std::string name, std::vector<sink> sinks) {
	std::vector<spdlog::sink_ptr> native_sinks;
	native_sinks.reserve(sinks.size());
	for (const auto &sink : sinks) {
		if (sink.impl_ && sink.impl_->native_) {
			native_sinks.push_back(sink.impl_->native_);
		}
	}
	return std::make_shared<spdlog::logger>(std::move(name), native_sinks.begin(), native_sinks.end());
}

logger logger::default_logger() {
	return logger(spdlog::default_logger());
}

logger logger::stderr_color(std::string name) {
	return logger(std::move(name), sink::stderr_color());
}

logger logger::stdout_color(std::string name) {
	return logger(std::move(name), sink::stdout_color());
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

void logger::log_message(level log_level, std::string_view message) const {
	if (!impl_) {
		return;
	}
	log_with_spdlog_logger(impl_, log_level, message);
}

void logger::trace(std::string_view message) const {
	log_message(level::trace, message);
}

void logger::debug(std::string_view message) const {
	log_message(level::debug, message);
}

void logger::info(std::string_view message) const {
	log_message(level::info, message);
}

void logger::warn(std::string_view message) const {
	log_message(level::warn, message);
}

void logger::error(std::string_view message) const {
	log_message(level::error, message);
}

void logger::critical(std::string_view message) const {
	log_message(level::critical, message);
}

void logger::set_level(level new_level) const {
	if (!impl_) {
		return;
	}
	impl_->set_level(detail::to_spdlog_level(new_level));
}

level logger::current_level() const noexcept {
	if (!impl_) {
		return level::off;
	}
	return from_spdlog_level(impl_->level());
}

void logger::flush_on(level flush_level) const {
	if (!impl_) {
		return;
	}
	impl_->flush_on(detail::to_spdlog_level(flush_level));
}

void logger::set_pattern(std::string_view pattern) const {
	if (!impl_) {
		return;
	}
	impl_->set_pattern(std::string(pattern));
}

void logger::set_as_default() const {
	if (!impl_) {
		return;
	}
	spdlog::set_default_logger(impl_);
}

logger default_logger() {
	return logger::default_logger();
}

void set_default_logger(const logger &new_default) {
	if (!new_default) {
		return;
	}
	spdlog::set_default_logger(new_default.impl_);
}

void set_level(level new_level) {
	spdlog::set_level(detail::to_spdlog_level(new_level));
}

void flush_on(level flush_level) {
	spdlog::flush_on(detail::to_spdlog_level(flush_level));
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

} // namespace warp::log
