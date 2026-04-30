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

using req_interceptor_result = std::optional<response>;

template<typename T>
concept valid_response_interceptor_response = std::same_as<std::remove_cvref_t<T>, void>;

template <typename Interceptor>
concept response_interceptor = requires(const std::decay_t<Interceptor> &i, response &resp) {
    requires valid_response_interceptor_response<decltype(i.intercept(resp))>;
};

} // namespace warp::http

namespace warp::server::detail {

using type_erased_req_interceptor = std::function<http::req_interceptor_result(request &)>;
using type_erased_resp_interceptor = std::function<void(response &)>;

template <typename T>
concept erased_interceptor_type =
    std::same_as<T, type_erased_req_interceptor> ||
    std::same_as<T, type_erased_resp_interceptor>;

} // namespace warp::server::detail
