#include "warp/net/util/status.hpp"

namespace warp::net::util {

const char* status_category::name() const noexcept {
    return "warp.http";
}

std::string status_category::message(int ev) const {
    switch (static_cast<status_code>(ev)) {
    case status_code::ok: return "ok";
    case status_code::cancelled: return "cancelled";
    case status_code::network_error: return "network error";
    case status_code::protocol_error: return "protocol error";
    case status_code::timeout: return "timeout";
    case status_code::internal_error: return "internal error";
    default: return "unknown";
    }
}

const std::error_category& status_category_instance() {
    static status_category instance;
    return instance;
}

std::error_code make_error(status_code code) {
    return {static_cast<int>(code), status_category_instance()};
}

} // namespace warp::net::util
