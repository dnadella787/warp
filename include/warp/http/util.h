//
// Created by Dhanush Nadella on 4/25/26.
//

#pragma once

namespace warp::http {

// value parameter
template <auto X>
inline constexpr bool always_false_v = false;

template <auto X>
inline constexpr bool always_true_v = true;

// type parameter
template <typename X>
inline constexpr bool always_false_t = false;

template <typename X>
inline constexpr bool always_true_t = true;

}
