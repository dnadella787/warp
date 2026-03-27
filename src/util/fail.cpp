//
// Created by Dhanush Nadella on 3/27/26.
//
#include "fail.h"

#include <iostream>

namespace warp::util {

void fail_except(boost::beast::error_code &ec, const std::string_view component, const std::string_view action) {
    fail(ec, component, action);
    throw new std::runtime_error(ec.message());
}

void fail(const std::string_view component, const std::string_view action, const std::string_view reason) {
    std::cerr << std::format("Error in %s during %s: %s", component, action, reason) << std::endl;
}

void fail(boost::beast::error_code &ec, const std::string_view component, const std::string_view action) {
    fail(component, action, ec.message());
}

}
