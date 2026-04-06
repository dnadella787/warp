//
// Created by Dhanush Nadella on 4/4/26.
//

#pragma once
#include <functional>

#include "request.hpp"
#include "response.hpp"

#include <boost/asio/awaitable.hpp>

namespace warp::http {

using headers = request::fields_type;
using method = boost::beast::http::verb;
template <typename T>
using awaitable = boost::asio::awaitable<T>;
using sync_handler = std::function<response(request)>; // copy by value so we can just move the req into user handler
using async_handler =
    std::function<awaitable<response>(request)>; // copy by value so we can just move the req into user handler

} // namespace warp::http
