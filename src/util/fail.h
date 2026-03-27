#pragma once
#include <boost/beast/core.hpp>

namespace warp::util {
    static void fail(boost::beast::error_code &ec, std::string_view component, std::string_view action, std::string_view reason) ;
    static void fail_except(boost::beast::error_code &ec, std::string_view component, std::string_view action, std::string_view reason) ;
}
