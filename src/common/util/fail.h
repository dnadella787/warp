#pragma once
#include <iostream>
#include <boost/beast/core.hpp>

namespace warp::util {

inline void fail(const std::string_view component, const std::string_view action, const std::string_view reason) {
	std::cerr << std::format("Error in {} during {}: {}", component, action, reason) << std::endl;
}

inline void fail(const boost::beast::error_code &ec, const std::string_view component, const std::string_view action) {
	fail(component, action, ec.message());
}

inline void fail_except(const boost::beast::error_code &ec, const std::string_view component, const std::string_view action) {
	fail(ec, component, action);
	throw new std::runtime_error(ec.message());
}

}
