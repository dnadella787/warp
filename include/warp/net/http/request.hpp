#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "warp/net/http/common.hpp"

namespace warp::net::http {

class json_value {
public:
    json_value();
    json_value(const json_value&);
    json_value(json_value&&) noexcept;
    json_value& operator=(const json_value&);
    json_value& operator=(json_value&&) noexcept;
    ~json_value();

    [[nodiscard]] bool valid() const noexcept;
    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] bool is_null() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_int64() const;
    [[nodiscard]] bool is_uint64() const;
    [[nodiscard]] bool is_double() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_object() const;

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_int64() const;
    [[nodiscard]] std::uint64_t as_uint64() const;
    [[nodiscard]] double as_double() const;
    [[nodiscard]] std::string as_string() const;

    [[nodiscard]] json_value at(std::string_view key) const;
    [[nodiscard]] json_value get(std::size_t index) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::string dump() const;

private:
    struct state;
    std::shared_ptr<state> state_;

    explicit json_value(std::shared_ptr<state> state) noexcept;
    [[nodiscard]] const state& require_state() const;
    friend class request;
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
    [[nodiscard]] json_value json_body() const;
    [[nodiscard]] std::optional<json_value> try_json_body() const noexcept;

private:
    method method_{method::unknown};
    std::string target_;
    std::string path_;
    std::string body_;
    headers headers_;
    std::unordered_map<std::string, std::string> path_params_;
    std::unordered_map<std::string, std::string> query_params_;
};

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

} // namespace warp::net::http
