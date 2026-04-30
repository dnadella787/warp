#include "server/interceptors/interceptor_chain.h"

#include <gtest/gtest.h>

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "warp/http/request.hpp"
#include "warp/http/response.hpp"

namespace {

using boost::beast::http::field;
using boost::beast::http::status;
using boost::beast::http::verb;
using warp::http::request;
using warp::http::response;
using warp::server::interceptor_chain;
using warp::server::detail::type_erased_req_interceptor;
using warp::server::detail::type_erased_resp_interceptor;

TEST(InterceptorChainTest, RequestChainRunsInOrderUntilAnInterceptorReturnsAResponse) {
	std::vector<std::string> events;
	interceptor_chain<request> chain(std::vector<type_erased_req_interceptor> {
	    [&events](request &req) -> std::optional<response> {
		    events.push_back("first");
		    req.set_path_params({{"id", "42"}});
		    return std::nullopt;
	    },
	    [&events](request &req) -> std::optional<response> {
		    events.push_back("second");
		    return response::forbidden(std::string(req.path_param("id").value_or("missing")));
	    },
	    [&events](request &) -> std::optional<response> {
		    events.push_back("third");
		    return std::nullopt;
	    }});

	request req(verb::get, "/items/42", 11);
	const auto result = chain.run(req);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->result(), status::forbidden);
	EXPECT_EQ(result->body(), R"({"error":"42"})");
	EXPECT_EQ(events, (std::vector<std::string> {"first", "second"}));
}

TEST(InterceptorChainTest, RequestChainReturnsNulloptWhenNoInterceptorShortCircuits) {
	std::vector<std::string> events;
	interceptor_chain<request> chain(
	    std::vector<type_erased_req_interceptor> {[&events](request &req) -> std::optional<response> {
		                                              events.push_back("first");
		                                              req.target("/changed?via=interceptor");
		                                              req.refresh_target_metadata();
		                                              return std::nullopt;
	                                              },
	                                              [&events](request &req) -> std::optional<response> {
		                                              events.push_back("second");
		                                              EXPECT_EQ(req.path(), "/changed");
		                                              EXPECT_EQ(req.query_param("via"), "interceptor");
		                                              return std::nullopt;
	                                              }});

	request req(verb::get, "/before", 11);
	const auto result = chain.run(req);

	EXPECT_FALSE(result.has_value());
	EXPECT_EQ(req.path(), "/changed");
	EXPECT_EQ(req.query_param("via"), "interceptor");
	EXPECT_EQ(events, (std::vector<std::string> {"first", "second"}));
}

TEST(InterceptorChainTest, ResponseChainRunsAllInterceptorsInOrderAndCanMutateResponse) {
	std::vector<std::string> events;
	interceptor_chain<response> chain(std::vector<type_erased_resp_interceptor> {[&events](response &resp) {
		                                                                             events.push_back("first");
		                                                                             resp.set(field::server,
		                                                                                      "warp-test");
	                                                                             },
	                                                                             [&events](response &resp) {
		                                                                             events.push_back("second");
		                                                                             resp.result(status::accepted);
	                                                                             }});

	response resp = response::ok(R"({"ok":true})");
	chain.run(resp);

	EXPECT_EQ(resp[field::server], "warp-test");
	EXPECT_EQ(resp.result(), status::accepted);
	EXPECT_EQ(events, (std::vector<std::string> {"first", "second"}));
}

} // namespace
