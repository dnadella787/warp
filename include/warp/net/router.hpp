#pragma once

#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "warp/net/http.hpp"

namespace warp::net::router {

using handler = std::function<warp::net::http::response(const warp::net::http::request&)>;

class registry {
public:
    registry() = default;
    registry(const registry& other);
    registry& operator=(const registry& other);
    void add(std::string path, handler h);
    [[nodiscard]] std::optional<handler> find(std::string_view path) const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, handler> routes_;
};

} // namespace warp::net::router

namespace warp::net::router {

inline registry::registry(const registry& other) {
    std::shared_lock lock(other.mutex_);
    routes_ = other.routes_;
}

inline registry& registry::operator=(const registry& other) {
    if (this == &other) {
        return *this;
    }
    std::unique_lock lock_this(mutex_);
    std::shared_lock lock_other(other.mutex_);
    routes_ = other.routes_;
    return *this;
}

inline void registry::add(std::string path, handler h) {
    std::unique_lock lock(mutex_);
    routes_[std::move(path)] = std::move(h);
}

inline std::optional<handler> registry::find(std::string_view path) const {
    std::shared_lock lock(mutex_);
    if (auto it = routes_.find(std::string(path)); it != routes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace warp::net::router
