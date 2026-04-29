//
// Created by Dhanush Nadella on 4/28/26.
//

#pragma once
#include <optional>

#include "warp/warp.hpp"

namespace warp::http {

using InterceptorResult = std::optional<response>;

// lvalue and const lvalue by ref bind to value param
template <typename H>
inline constexpr bool is_movable_request_interceptor = requires(std::decay_t<H> &fn, request req) {
    { std::invoke(fn, std::move(req)) } -> std::convertible_to<void>;
};

// lvalue and const lvalue by ref bind to value param
template <typename H>
inline constexpr bool is_lvalue_request_interceptor = requires(std::decay_t<H> &fn, request &req) {
    { std::invoke(fn, req) } -> std::convertible_to<void>;
};

// request_interceptor
template <typename H>
inline constexpr bool is_request_interceptor = is_movable_request_interceptor<H> || is_lvalue_request_interceptor<H>;

// normalized interceptor class
template <typename Interceptor>
class interceptor {
public:
    explicit interceptor(Interceptor interceptor)
        : interceptor_(std::move(interceptor)) {}

    InterceptorResult operator()(warp::request& req) {
        using result_t = std::invoke_result_t<decltype(&Interceptor::intercept), Interceptor&, request&>;

        if constexpr (std::is_void_v<result_t>) {
            interceptor_.intercept(req);
            return std::nullopt;
        } else {
            return interceptor_.intercept(req);
        }
    }

private:
    Interceptor interceptor_;
};

}
