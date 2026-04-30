//
// Created by Dhanush Nadella on 4/30/26.
//

#pragma once
#include <utility>

namespace warp::common {

template <typename T, typename... Args>
concept can_be_built_with = requires(Args&&... args) {
    T(std::forward<Args>(args)...);
};

}
