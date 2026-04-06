#pragma once

#include "warp/http/http.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>

#include <string_view>

#include "common/util/lambda.h"

namespace warp::test {

inline http::response run_handler(const http::handler &handler, http::request req) {
	return std::visit(common::overloaded {[&](const http::sync_handler &h) { return h(std::move(req)); },
	                                      [&](const http::async_handler &h) {
		                                      boost::asio::io_context ioc;
		                                      auto future = boost::asio::co_spawn(ioc, h(std::move(req)),
		                                                                          boost::asio::use_future);
		                                      ioc.run();
		                                      return future.get();
	                                      }},
	                  handler);
}

inline boost::json::object parse_json_object(std::string_view json_text) {
	return boost::json::parse(json_text).as_object();
}

} // namespace warp::test
