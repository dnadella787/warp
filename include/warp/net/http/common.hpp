#pragma once

#include <string>
#include <unordered_map>

namespace warp::net::http {

using headers = std::unordered_map<std::string, std::string>;

enum class method {
    get,
    post,
    put,
    delete_,
    head,
    options,
    patch,
    unknown
};

} // namespace warp::net::http
