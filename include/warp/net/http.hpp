#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/json/value.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/error.hpp>

#include "warp/net/util/status.hpp"

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

class request {
public:
    request() = default;
    request(method m, std::string target, std::string body, headers hdrs = {});

    [[nodiscard]] method verb() const noexcept;
    [[nodiscard]] std::string_view target() const noexcept;
    void set_path(std::string path);
    [[nodiscard]] std::string_view path() const noexcept;
    void set_query_params(std::unordered_map<std::string, std::string> params);
    [[nodiscard]] const std::unordered_map<std::string, std::string>& query_params() const noexcept;
    [[nodiscard]] std::optional<std::string_view> query_param(std::string_view key) const;
    [[nodiscard]] std::string_view body() const noexcept;
    [[nodiscard]] const headers& header_map() const noexcept;

    void set_path_params(std::unordered_map<std::string, std::string> params);
    [[nodiscard]] const std::unordered_map<std::string, std::string>& path_params() const noexcept;
    [[nodiscard]] std::optional<std::string_view> path_param(std::string_view key) const;
    [[nodiscard]] boost::json::value json_body() const;
    [[nodiscard]] std::optional<boost::json::value> try_json_body() const noexcept;

private:
    method method_{method::unknown};
    std::string target_;
    std::string path_;
    std::string body_;
    headers headers_;
    std::unordered_map<std::string, std::string> path_params_;
    std::unordered_map<std::string, std::string> query_params_;
};

class response {
public:
    response() = default;

    static response ok(std::string body);
    static response not_found();
    static response server_error(std::string message);

    response& set_header(std::string key, std::string value);
    [[nodiscard]] std::string_view body() const noexcept;
    [[nodiscard]] const headers& header_map() const noexcept;
    [[nodiscard]] unsigned status() const noexcept;

private:
    response(unsigned status, std::string body);

    unsigned status_{200};
    std::string body_;
    headers headers_;
};

struct http_result {
    response resp;
    warp::net::util::error_info error{};
};

} // namespace warp::net::http

namespace warp::net::http {

inline request::request(method m, std::string target, std::string body, headers hdrs)
    : method_(m)
    , target_(std::move(target))
    , body_(std::move(body))
    , headers_(std::move(hdrs)) {}

inline method request::verb() const noexcept {
    return method_;
}

inline std::string_view request::target() const noexcept {
    return target_;
}

inline void request::set_path(std::string path) {
    path_ = std::move(path);
}

inline std::string_view request::path() const noexcept {
    return path_;
}

inline void request::set_query_params(std::unordered_map<std::string, std::string> params) {
    query_params_ = std::move(params);
}

inline const std::unordered_map<std::string, std::string>& request::query_params() const noexcept {
    return query_params_;
}

inline std::optional<std::string_view> request::query_param(std::string_view key) const {
    if (auto it = query_params_.find(std::string(key)); it != query_params_.end()) {
        return it->second;
    }
    return std::nullopt;
}

inline std::string_view request::body() const noexcept {
    return body_;
}

inline const headers& request::header_map() const noexcept {
    return headers_;
}

inline void request::set_path_params(std::unordered_map<std::string, std::string> params) {
    path_params_ = std::move(params);
}

inline const std::unordered_map<std::string, std::string>& request::path_params() const noexcept {
    return path_params_;
}

inline std::optional<std::string_view> request::path_param(std::string_view key) const {
    if (auto it = path_params_.find(std::string(key)); it != path_params_.end()) {
        return it->second;
    }
    return std::nullopt;
}

inline boost::json::value request::json_body() const {
    return boost::json::parse(body_);
}

inline std::optional<boost::json::value> request::try_json_body() const noexcept {
    try {
        return boost::json::parse(body_);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

inline response::response(unsigned status, std::string body)
    : status_(status)
    , body_(std::move(body)) {}

inline response response::ok(std::string body) {
    return response(200, std::move(body));
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

inline response& response::set_header(std::string key, std::string value) {
    headers_[std::move(key)] = std::move(value);
    return *this;
}

inline std::string_view response::body() const noexcept {
    return body_;
}

inline const headers& response::header_map() const noexcept {
    return headers_;
}

inline unsigned response::status() const noexcept {
    return status_;
}

} // namespace warp::net::http
