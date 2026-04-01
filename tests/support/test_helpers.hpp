#pragma once

#include "warp/http/server.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>

#include <string_view>

namespace warp::test {

inline response run_handler(const async_handler &handler, request req) {
	boost::asio::io_context ioc;
	auto future = boost::asio::co_spawn(ioc, handler(std::move(req)), boost::asio::use_future);
	ioc.run();
	return future.get();
}

inline boost::json::object parse_json_object(std::string_view json_text) {
	return boost::json::parse(json_text).as_object();
}

} // namespace warp::test
