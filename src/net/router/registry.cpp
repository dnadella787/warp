#include "registry.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace warp::net::router {

registry::registry(const registry& other) {
    std::shared_lock lock(other.mutex_);
    routes_ = other.routes_;
}

registry& registry::operator=(const registry& other) {
    if (this == &other) {
        return *this;
    }
    std::unique_lock lock_this(mutex_);
    std::shared_lock lock_other(other.mutex_);
    routes_ = other.routes_;
    return *this;
}

void registry::add(std::string path, handler h) {
    auto segments = compile_pattern(path);
    std::unique_lock lock(mutex_);
    // Replace existing entry with same pattern if present.
    for (auto& route : routes_) {
        if (route.pattern == path) {
            route.handler = std::move(h);
            route.segments = std::move(segments);
            return;
        }
    }
    routes_.push_back(route_entry{
        .pattern = std::move(path),
        .segments = std::move(segments),
        .handler = std::move(h)});
}

std::optional<match_result> registry::find(std::string_view path) const {
    auto clean_path = path.substr(0, path.find('?'));
    auto path_segments = split_path(clean_path);
    const bool invalid_path = path_segments.empty() && !(clean_path.empty() || clean_path == "/");
    if (invalid_path) {
        return std::nullopt;
    }

    std::shared_lock lock(mutex_);
    for (const auto& route : routes_) {
        if (route.segments.size() != path_segments.size()) {
            continue;
        }
        std::unordered_map<std::string, std::string> params;
        if (match_segments(route.segments, path_segments, params)) {
            return match_result{route.handler, std::move(params)};
        }
    }
    return std::nullopt;
}

std::vector<registry::segment> registry::compile_pattern(const std::string& pattern) {
    if (pattern.empty() || pattern.front() != '/') {
        throw std::invalid_argument("route pattern must start with '/'");
    }
    if (pattern == "/") {
        return {};
    }

    std::vector<segment> segments;
    std::size_t start = 1;
    while (start <= pattern.size()) {
        auto end = pattern.find('/', start);
        auto token = pattern.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (token.empty()) {
            throw std::invalid_argument("route pattern contains empty segment");
        }
        if (token.front() == '{' && token.back() == '}') {
            if (token.size() <= 2) {
                throw std::invalid_argument("route parameter name cannot be empty");
            }
            std::string name = token.substr(1, token.size() - 2);
            segments.push_back(segment{segment::kind::parameter, std::move(name)});
        } else {
            segments.push_back(segment{segment::kind::literal, token});
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return segments;
}

std::vector<std::string_view> registry::split_path(std::string_view path) {
    std::vector<std::string_view> segments;
    std::size_t pos = 0;
    if (!path.empty() && path.front() == '/') {
        pos = 1;
    }
    if (pos >= path.size()) {
        return segments;
    }
    while (pos <= path.size()) {
        auto next = path.find('/', pos);
        auto len = (next == std::string::npos) ? path.size() - pos : next - pos;
        if (len == 0) {
            return {};
        }
        segments.emplace_back(path.substr(pos, len));
        if (next == std::string::npos) {
            break;
        }
        pos = next + 1;
    }
    return segments;
}

bool registry::match_segments(
    const std::vector<segment>& pattern_segments,
    const std::vector<std::string_view>& path_segments,
    std::unordered_map<std::string, std::string>& out_params) {
    for (std::size_t i = 0; i < pattern_segments.size(); ++i) {
        const auto& seg = pattern_segments[i];
        const auto& value = path_segments[i];
        if (seg.type == segment::kind::literal) {
            if (value != seg.value) {
                return false;
            }
        } else {
            out_params.emplace(seg.value, std::string(value));
        }
    }
    return true;
}

} // namespace warp::net::router
