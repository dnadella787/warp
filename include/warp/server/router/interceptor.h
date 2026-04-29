//
// Created by Dhanush Nadella on 4/28/26.
//

#pragma once
#include <optional>

#include "warp/warp.hpp"

namespace warp::http {

template<typename T>
concept valid_request_interceptor_response = std::same_as<T, void> || std::same_as<T, response> || std::same_as<T, std::optional<response>>;

// has to be by ref since we reuse the same request in the handler dispatch
// so we cannot rely on move semantics for zero alloc dispatch like we can do with
// handlers
template <typename Interceptor>
concept request_interceptor = requires(std::decay_t<Interceptor> &i, request &req) {
    requires valid_request_interceptor_response<decltype(i.intercept(req))>;
};

using interceptor_result = std::optional<response>;

}
