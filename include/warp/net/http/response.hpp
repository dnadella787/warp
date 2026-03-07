#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "warp/net/http/common.hpp"

namespace warp::net::http {

class response {
public:
    response() = default;

    static response ok(std::string body);
    static response not_found();
    static response server_error(std::string message);

    response &set_header(std::string key, std::string value);
    [[nodiscard]] std::string_view body() const noexcept;
    [[nodiscard]] const headers &header_map() const noexcept;
    [[nodiscard]] unsigned status() const noexcept;

private:
    response(unsigned status, std::string body);

    unsigned status_{200};
    std::string body_;
    headers headers_;
};

inline response::response(unsigned status, std::string body)
    : status_(status), body_(std::move(body)) {}

inline response response::ok(std::string body) {
    return {200, std::move(body)};
}

inline response response::not_found() {
    response r(404, "Not Found");
    r.set_header("Content-Type", "text/plain");
    return r;
}

inline response response::server_error(std::string message) {
    response r(500, std::move(message));
    r.set_header("Content-Type", "text/plain");
    return r;
}

inline response &response::set_header(std::string key, std::string value) {
    headers_[std::move(key)] = std::move(value);
    return *this;
}

inline std::string_view response::body() const noexcept {
    return body_;
}

inline const headers &response::header_map() const noexcept {
    return headers_;
}

inline unsigned response::status() const noexcept {
    return status_;
}

} // namespace warp::net::http
