#include "warp/net/http/request.hpp"

#include "warp/net/http/json_value.hpp"

#include <exception>
#include <optional>

namespace warp::net::http {

json_value request::json_body() const {
    return json_value::parse(body_);
}

std::optional<json_value> request::try_json_body() const noexcept {
    try {
        return json_value::parse(body_);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace warp::net::http
