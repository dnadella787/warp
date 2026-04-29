#include "warp/logging/logger.hpp"

#include <gtest/gtest.h>

namespace {

TEST(LoggerTest, CreatesNamedLoggersAndPreservesNames) {
	auto stderr_logger = warp::log::logger::stderr_color("warp.logger.test.stderr");
	auto stdout_logger = warp::log::logger::stdout_color("warp.logger.test.stdout");

	EXPECT_TRUE(stderr_logger);
	EXPECT_TRUE(stdout_logger);
	EXPECT_EQ(stderr_logger.name(), "warp.logger.test.stderr");
	EXPECT_EQ(stdout_logger.name(), "warp.logger.test.stdout");
}

TEST(LoggerTest, DefaultLoggerCanBeReplacedAndConfiguredThroughWrapper) {
	auto original = warp::log::default_logger();
	auto test_logger = warp::log::logger::stderr_color("warp.logger.test.default");

	warp::log::set_default_logger(test_logger);
	warp::log::set_level(warp::log::level::warn);
	warp::log::flush_on(warp::log::level::error);

	EXPECT_EQ(warp::log::default_logger().name(), "warp.logger.test.default");
	EXPECT_EQ(warp::log::default_logger().current_level(), warp::log::level::warn);

	warp::log::set_default_logger(original);
}

} // namespace
