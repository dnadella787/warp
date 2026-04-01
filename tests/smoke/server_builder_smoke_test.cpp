#include "warp/http/server.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {

constexpr std::array event_loop_modes {
    warp::event_loop_mode::callbacks,
    warp::event_loop_mode::coroutines,
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

} // namespace
