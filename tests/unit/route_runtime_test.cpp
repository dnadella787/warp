#include "server/router/route_runtime.hpp"

#include <gtest/gtest.h>

#include <boost/beast/http/verb.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using boost::beast::http::verb;
using warp::http::method;
using warp::http::request;
using warp::http::response;
using warp::server::route_runtime;
using warp::server::detail::route_definition;

struct fake_session {
	enum class dispatch_kind {
		none,
		sync,
		async,
	};

	void dispatch_sync_handler(std::size_t sequence, const warp::http::sync_handler &, request request) {
		last_dispatch = dispatch_kind::sync;
		last_sequence = sequence;
		last_target = std::string(request.target());
		if (const auto value = request.path_param("id"); value.has_value()) {
			last_path_param = std::string(*value);
		} else {
			last_path_param.reset();
		}
	}

	void dispatch_async_handler(std::size_t sequence, const warp::http::async_handler &, request request) {
		last_dispatch = dispatch_kind::async;
		last_sequence = sequence;
		last_target = std::string(request.target());
		if (const auto value = request.path_param("id"); value.has_value()) {
			last_path_param = std::string(*value);
		} else {
			last_path_param.reset();
		}
	}

	dispatch_kind last_dispatch {dispatch_kind::none};
	std::size_t last_sequence {};
	std::string last_target;
	std::optional<std::string> last_path_param;
};

route_definition make_sync_route(method verb_value, std::string path,
                                 std::vector<warp::http::query_constraint_descriptor> constraints = {}) {
	return route_definition {
	    .verb = verb_value,
	    .path = std::move(path),
	    .typed_query_constraints = std::move(constraints),
	    .callback = warp::http::sync_handler {[](request) -> response { return response::ok("sync"); }},
	};
}

route_definition make_async_route(method verb_value, std::string path,
                                  std::vector<warp::http::query_constraint_descriptor> constraints = {}) {
	return route_definition {
	    .verb = verb_value,
	    .path = std::move(path),
	    .typed_query_constraints = std::move(constraints),
	    .callback = warp::http::async_handler {[](request) -> warp::http::awaitable<response> {
		    co_return response::accepted("async");
	    }},
	};
}

TEST(RouteRuntimeTest, DispatchesPreboundSyncAndAsyncHandlers) {
	route_runtime<fake_session> routes(
	    {make_sync_route(method::get, "/users/{id}"), make_async_route(method::post, "/jobs")});

	fake_session session;

	request sync_request(verb::get, "/users/42", 11);
	const auto sync_match = routes.find(sync_request);
	ASSERT_TRUE(sync_match.has_value());
	routes.dispatch(sync_match->id, session, 7, std::move(sync_request));
	EXPECT_EQ(session.last_dispatch, fake_session::dispatch_kind::sync);
	EXPECT_EQ(session.last_sequence, 7U);
	EXPECT_EQ(session.last_path_param, std::optional<std::string> {"42"});

	request async_request(verb::post, "/jobs", 11);
	const auto async_match = routes.find(async_request);
	ASSERT_TRUE(async_match.has_value());
	routes.dispatch(async_match->id, session, 9, std::move(async_request));
	EXPECT_EQ(session.last_dispatch, fake_session::dispatch_kind::async);
	EXPECT_EQ(session.last_sequence, 9U);
	EXPECT_FALSE(session.last_path_param.has_value());
}

TEST(RouteRuntimeTest, DuplicateRoutesFailDuringRuntimeConstruction) {
	EXPECT_THROW((route_runtime<fake_session> {
	                 {make_sync_route(method::get, "/health"), make_sync_route(method::get, "/health")}}),
	             std::invalid_argument);
}

} // namespace
