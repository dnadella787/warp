#include "warp/logging/logger.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

TEST(LoggerTest, SupportsExplicitSinkConstructionApis) {
	auto stderr_logger = warp::log::logger::stderr_color("warp.logger.test.stderr");
	auto stdout_logger = warp::log::logger::stdout_color("warp.logger.test.stdout");
	auto file_logger =
	    warp::log::logger("warp.logger.test.file", warp::log::sink::basic_file("/tmp/warp_logger_test.log", true));
	auto single_sink_logger = warp::log::logger("warp.logger.test.single", warp::log::sink::stderr_color());
	auto init_list_logger =
	    warp::log::logger("warp.logger.test.list", {warp::log::sink::basic_file("/tmp/warp_logger_list_test.log", true),
	                                                warp::log::sink::stdout_color(), warp::log::sink::stderr_color()});
	std::vector<warp::log::sink> sinks {warp::log::sink::stdout_color(), warp::log::sink::stderr_color()};
	auto range_logger = warp::log::logger("warp.logger.test.range", sinks.begin(), sinks.end());

	EXPECT_TRUE(stderr_logger);
	EXPECT_TRUE(stdout_logger);
	EXPECT_TRUE(file_logger);
	EXPECT_TRUE(single_sink_logger);
	EXPECT_TRUE(init_list_logger);
	EXPECT_TRUE(range_logger);
	EXPECT_EQ(stderr_logger.name(), "warp.logger.test.stderr");
	EXPECT_EQ(stdout_logger.name(), "warp.logger.test.stdout");
	EXPECT_EQ(file_logger.name(), "warp.logger.test.file");
	EXPECT_EQ(single_sink_logger.name(), "warp.logger.test.single");
	EXPECT_EQ(init_list_logger.name(), "warp.logger.test.list");
	EXPECT_EQ(range_logger.name(), "warp.logger.test.range");
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

TEST(LoggerTest, ExplicitSinkLoggerCanInstallItselfAsDefault) {
	auto original = warp::log::default_logger();
	auto test_logger = warp::log::logger::stdout_color("warp.logger.test.set_as_default");

	test_logger.set_as_default();

	EXPECT_EQ(warp::log::default_logger().name(), "warp.logger.test.set_as_default");

	warp::log::set_default_logger(original);
}

} // namespace
