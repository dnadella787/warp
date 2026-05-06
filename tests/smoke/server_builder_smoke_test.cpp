#include <gtest/gtest.h>

#include <array>
#include <thread>

#include "server/server_impl.hpp"

#include "warp/warp.hpp"
#include "warp/server/server.hpp"
#include "warp/server/server_builder.hpp"

namespace warp::server::detail {

struct server_test_access {
	template <warp::event_loop_mode Mode>
	static const warp::log::logger &captured_logger(const warp::server::server &server) {
		using impl_t = warp::server::server::server_impl<Mode>;
		const auto impl = std::static_pointer_cast<impl_t>(server.impl_);
		return impl->logger_;
	}
};

} // namespace warp::server::detail

namespace {

class default_logger_guard {
public:
	default_logger_guard() : original_(warp::log::default_logger()) {
	}

	~default_logger_guard() {
		warp::log::set_default_logger(original_);
	}

private:
	warp::log::logger original_;
};

template <warp::event_loop_mode Mode>
const warp::log::logger &captured_server_logger(const warp::server::server &server) {
	return warp::server::detail::server_test_access::captured_logger<Mode>(server);
}

struct mutable_resource {
	void register_routes(warp::server::server_builder &builder) {
		builder.get<"/resource/health">([](const warp::request &) -> warp::response {
			return warp::response::ok(warp::body_builder().set("route", "resource-health").build());
		});
	}
};

struct const_resource {
	void register_routes(warp::server::server_builder &builder) const {
		builder.get<"/resource/const">([](const warp::request &) -> warp::response {
			return warp::response::ok(warp::body_builder().set("route", "resource-const").build());
		});
	}
};

template <warp::event_loop_mode Mode>
void expect_server_builds_for_mode() {
	auto server =
	    warp::server::server_builder()
	        .address("127.0.0.1")
	        .port(8081)
	        .worker_threads(2)
	        .get<"/health">([](const warp::request &) -> warp::response {
		        return warp::response::ok(warp::body_builder().set("route", "health").build());
	        })
	        .template post<"/jobs">([](warp::request) -> warp::awaitable<warp::response> {
		        co_return warp::response::accepted(warp::body_builder().set("queued", true).build());
	        })
	        .template get<"/reports/{report_id}", warp::http::required_query<"summary">>(
	            [](const warp::request &req) -> warp::response {
		            return warp::response::ok(
		                warp::body_builder().set("report_id", req.path_param("report_id").value_or("")).build());
	            })
	        .template delete_<"/jobs/{id}">([](const warp::request &req) -> warp::response {
		        return warp::response::ok(
		            warp::body_builder().set("deleted", true).set("id", req.path_param("id").value_or("")).build());
	        })
	        .template build<Mode>();
	server.stop();
}

TEST(ServerBuilderSmokeTest, BuildsServerWithSyncAndAsyncRoutesForBothEventLoopModes) {
	expect_server_builds_for_mode<warp::event_loop_mode::callbacks>();
	expect_server_builds_for_mode<warp::event_loop_mode::coroutines>();
}

TEST(ServerBuilderSmokeTest, RegistersMutableAndConstResources) {
	warp::server::server_builder builder;
	mutable_resource resource;
	const const_resource const_resource_instance;

	auto &configured = builder.register_resource(resource).register_resource(const_resource_instance);

	EXPECT_EQ(&configured, &builder);
}

TEST(ServerBuilderSmokeTest, AcceptsExplicitLoggerConfiguration) {
	warp::server::server_builder builder;
	auto custom_logger = warp::log::logger::stderr_color("warp.server_builder.test");

	auto &configured = builder.logger(custom_logger);
	EXPECT_EQ(&configured, &builder);

	auto server =
	    builder.get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
	        .build<warp::event_loop_mode::callbacks>();
	EXPECT_EQ(captured_server_logger<warp::event_loop_mode::callbacks>(server).name(), custom_logger.name());
	server.stop();
}

template <warp::event_loop_mode Mode>
void expect_default_logger_is_captured_at_build_time() {
	default_logger_guard restore_default;
	auto first_default = warp::log::logger::stderr_color("warp.server_builder.default.first");
	auto second_default = warp::log::logger::stdout_color("warp.server_builder.default.second");

	warp::log::set_default_logger(first_default);
	auto first_server =
	    warp::server::server_builder()
	        .port(0)
	        .get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
	        .template build<Mode>();

	warp::log::set_default_logger(second_default);
	auto second_server =
	    warp::server::server_builder()
	        .port(0)
	        .get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
	        .template build<Mode>();

	EXPECT_EQ(captured_server_logger<Mode>(first_server).name(), first_default.name());
	EXPECT_EQ(captured_server_logger<Mode>(second_server).name(), second_default.name());

	first_server.stop();
	second_server.stop();
}

template <warp::event_loop_mode Mode>
void expect_explicit_logger_is_not_retargeted_by_default_logger_changes() {
	default_logger_guard restore_default;
	auto initial_default = warp::log::logger::stderr_color("warp.server_builder.default.initial");
	auto replacement_default = warp::log::logger::stdout_color("warp.server_builder.default.replacement");
	auto explicit_logger = warp::log::logger("warp.server_builder.explicit",
	                                         {warp::log::sink::stderr_color(), warp::log::sink::stdout_color()});

	warp::log::set_default_logger(initial_default);
	auto server = warp::server::server_builder()
	                  .port(0)
	                  .logger(explicit_logger)
	                  .get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
	                  .template build<Mode>();

	warp::log::set_default_logger(replacement_default);

	EXPECT_EQ(captured_server_logger<Mode>(server).name(), explicit_logger.name());

	server.stop();
}

TEST(ServerBuilderSmokeTest, OmittingLoggerCapturesCurrentDefaultLoggerAtBuildTime) {
	expect_default_logger_is_captured_at_build_time<warp::event_loop_mode::callbacks>();
	expect_default_logger_is_captured_at_build_time<warp::event_loop_mode::coroutines>();
}

TEST(ServerBuilderSmokeTest, LaterDefaultLoggerChangesDoNotRetargetExplicitBuilderLoggers) {
	expect_explicit_logger_is_not_retargeted_by_default_logger_changes<warp::event_loop_mode::callbacks>();
	expect_explicit_logger_is_not_retargeted_by_default_logger_changes<warp::event_loop_mode::coroutines>();
}

TEST(ServerBuilderSmokeTest, BuildsServerFromTypedRouteSpecs) {
	auto server = warp::server::server_builder()
	                  .get<"/reports/{report_id}", warp::http::required_query<"summary">>(
	                      [](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
	                  .build();

	server.stop();
}

template <warp::event_loop_mode Mode>
void expect_duplicate_routes_fail_at_build_time() {
	const auto build_server = []() {
		return warp::server::server_builder()
		    .get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
		    .template get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
		    .template build<Mode>();
	};

	EXPECT_THROW(static_cast<void>(build_server()), std::invalid_argument);
}

TEST(ServerBuilderSmokeTest, DuplicateRoutesStillFailWhenServerBuildOwnsRuntimeConstruction) {
	expect_duplicate_routes_fail_at_build_time<warp::event_loop_mode::callbacks>();
	expect_duplicate_routes_fail_at_build_time<warp::event_loop_mode::coroutines>();
}

TEST(ServerBuilderSmokeTest, ConcurrentRunAndStopDoNotRaceServerLifecycle) {
	for (int iteration = 0; iteration < 25; ++iteration) {
		auto callbacks_server =
		    warp::server::server_builder()
		        .address("127.0.0.1")
		        .port(0)
		        .worker_threads(2)
		        .get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
		        .build<warp::event_loop_mode::callbacks>();

		std::thread callbacks_runner([&callbacks_server]() { callbacks_server.run(false); });
		std::thread callbacks_stopper([&callbacks_server]() { callbacks_server.stop(); });

		callbacks_runner.join();
		callbacks_stopper.join();
		callbacks_server.stop();

		auto coroutines_server =
		    warp::server::server_builder()
		        .address("127.0.0.1")
		        .port(0)
		        .worker_threads(2)
		        .get<"/health">([](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
		        .build<warp::event_loop_mode::coroutines>();

		std::thread coroutines_runner([&coroutines_server]() { coroutines_server.run(false); });
		std::thread coroutines_stopper([&coroutines_server]() { coroutines_server.stop(); });

		coroutines_runner.join();
		coroutines_stopper.join();
		coroutines_server.stop();
	}
}

} // namespace
