#include "warp/http/server.hpp"

#include <gtest/gtest.h>

#include <array>
#include <thread>

#include "warp/warp.hpp"
#include "warp/http/server_builder.hpp"

namespace {

constexpr std::array event_loop_modes {
    warp::event_loop_mode::callbacks,
    warp::event_loop_mode::coroutines,
};

struct mutable_resource {
	void register_routes(warp::http::server_builder &builder) {
		builder.get("/resource/health", [](const warp::request &) -> warp::response {
			return warp::response::ok(warp::body_builder().set("route", "resource-health").build());
		});
	}
};

struct const_resource {
	void register_routes(warp::http::server_builder &builder) const {
		builder.get("/resource/const", [](const warp::request &) -> warp::response {
			return warp::response::ok(warp::body_builder().set("route", "resource-const").build());
		});
	}
};

TEST(ServerBuilderSmokeTest, BuildsServerWithSyncAndAsyncRoutesForBothEventLoopModes) {
	for (auto mode : event_loop_modes) {
		warp::http::server_builder builder;
		auto &configured =
		    builder.address("127.0.0.1")
		        .port(8081)
		        .worker_threads(2)
		        .event_loop(mode)
		        .get("/health",
		             [](const warp::request &) -> warp::response {
			             return warp::response::ok(warp::body_builder().set("route", "health").build());
		             })
		        .post("/jobs",
		              [](warp::request) -> warp::awaitable<warp::response> {
			              co_return warp::response::accepted(warp::body_builder().set("queued", true).build());
		              })
		        .delete_("/jobs/{id}", [](const warp::request &req) -> warp::response {
			        return warp::response::ok(
			            warp::body_builder().set("deleted", true).set("id", req.path_param("id").value_or("")).build());
		        });

		EXPECT_EQ(&configured, &builder);
	}
}

TEST(ServerBuilderSmokeTest, RegistersMutableAndConstResources) {
	warp::http::server_builder builder;
	mutable_resource resource;
	const const_resource const_resource_instance;

	auto &configured = builder.register_resource(resource).register_resource(const_resource_instance);

	EXPECT_EQ(&configured, &builder);
}

TEST(ServerBuilderSmokeTest, ConcurrentRunAndStopDoNotRaceServerLifecycle) {
	for (auto mode : event_loop_modes) {
		for (int iteration = 0; iteration < 25; ++iteration) {
			auto server =
			    warp::http::server_builder()
			        .address("127.0.0.1")
			        .port(0)
			        .worker_threads(2)
			        .event_loop(mode)
			        .get("/health", [](const warp::request &) -> warp::response { return warp::response::ok("ok"); })
			        .build();

			std::thread runner([&server]() { server.run(false); });
			std::thread stopper([&server]() { server.stop(); });

			runner.join();
			stopper.join();
			server.stop();
		}
	}
}

} // namespace
