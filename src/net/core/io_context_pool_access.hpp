#pragma once

#include <boost/asio/io_context.hpp>

#include "warp/net/core/io_context_pool.hpp"

namespace warp::net::core::detail {

struct executor_access {
    static boost::asio::io_context& context(const io_context_pool::executor& exec) {
        auto* ptr = static_cast<boost::asio::io_context*>(exec.context_);
        return *ptr;
    }
};

} // namespace warp::net::core::detail
