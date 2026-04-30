//
// Created by Dhanush Nadella on 4/28/26.
//

#pragma once
#include <functional>
#include <optional>
#include <type_traits>

#include "warp/warp.hpp"

namespace warp::http {

template<typename T>
concept valid_request_interceptor_response = std::same_as<std::remove_cvref_t<T>, void> ||
                                             std::same_as<std::remove_cvref_t<T>, response> ||
                                             std::same_as<std::remove_cvref_t<T>, std::optional<response>>;

// has to be by ref since we reuse the same request in the handler dispatch
// so we cannot rely on move semantics for zero alloc dispatch like we can do with
// handlers
template <typename Interceptor>
concept request_interceptor = requires(const std::decay_t<Interceptor> &i, request &req) {
    requires valid_request_interceptor_response<decltype(i.intercept(req))>;
};

using interceptor_result = std::optional<response>;

} // namespace warp::http

namespace warp::server::detail {

using type_erased_interceptor = std::function<http::interceptor_result(request &)>;

} // namespace warp::server::detail
