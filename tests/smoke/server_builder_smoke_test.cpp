#include "warp/server/server.hpp"

#include <gtest/gtest.h>

#include <array>
#include <thread>

#include "warp/warp.hpp"
#include "warp/server/server_builder.hpp"

namespace {

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

TEST(ServerBuilderSmokeTest, BuildsServerFromTypedRouteSpecs) {
	auto server = warp::server::server_builder()
	                  .get<"/reports/{report_id}", warp::http::required_query<"summary">>(
	                      [](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
	                  .build();

	server.stop();
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
