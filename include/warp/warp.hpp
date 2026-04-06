//
// Created by Dhanush Nadella on 4/4/26.
//
#pragma once

#include "http/http.hpp"
#include "http/event_loop_mode.hpp"
#include "http/response_builder.hpp"

namespace warp {
using body_builder = http::body_builder;
using response_builder = http::response_builder;
using request = http::request;
using response = http::response;
using headers = http::headers;
using method = http::method;
template <typename T>
using awaitable = http::awaitable<T>;
using handler = http::sync_handler;
using async_handler = http::async_handler;
using event_loop_mode = http::event_loop_mode;
} // namespace warp
