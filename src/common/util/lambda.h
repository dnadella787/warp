//
// Created by Dhanush Nadella on 4/5/26.
//

#pragma once

namespace warp::common {

/* This is a utility class so that we can use std::visit to iterate over multiple
 * lambdas within a single expression and choose the proper one based on the actual
 * value of std::variant.
 *
 * overloaded inherits from a variadic template list. So if it is comprised of 4 lambdas,
 * it inherits all their () implementations. So the multiple possibilities in
 * std::variant will be mapped to the proper lambda executor based on the type match for the lambda
 * input and variant value.
 */
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

/* This is to help the compiler understand how to make sense of
 *
 *     overloaded{
 *       [](char x) -> std::string {return "a"},
 *       [](int x) -> std::string {return "b"}
 *     };
 *
 * and properly turn it into overloaded<LambdaType1, LambdaType2>
 * where LambdaType1 and LambdaType2 are the lambdas' unique closure types.
 */
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}