//
// Created by Dhanush Nadella on 4/5/26.
//

#pragma once

namespace warp::common {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}