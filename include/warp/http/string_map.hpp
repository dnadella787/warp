#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace warp::http {

struct transparent_string_hash {
	using is_transparent = void;

	[[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
		return std::hash<std::string_view> {}(value);
	}

	[[nodiscard]] std::size_t operator()(const std::string &value) const noexcept {
		return (*this)(std::string_view {value});
	}
};

struct transparent_string_equal {
	using is_transparent = void;

	[[nodiscard]] bool operator()(const std::string &lhs, const std::string &rhs) const noexcept {
		return lhs == rhs;
	}

	[[nodiscard]] bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
		return lhs == rhs;
	}

	[[nodiscard]] bool operator()(const std::string &lhs, std::string_view rhs) const noexcept {
		return std::string_view {lhs} == rhs;
	}

	[[nodiscard]] bool operator()(std::string_view lhs, const std::string &rhs) const noexcept {
		return lhs == std::string_view {rhs};
	}
};

/*
 * helper class that allows hashing + comparison of both std::string and std::string_view
 * otherwise you would have to do m.find(std::string{"test"}). This is useful for reducing allocations
 * on request param lookups.
 *
 * transparent_string_hash provides a way to hash both strings and string_view
 *
 * transparent_string_equal provides a way to compare both strings and string_views by giving
 * allocation free comparisons for:
 *
 * std::string == std::string
 * std::string_view == std::string_view
 * std::string == std::string_view
 * std::string_view == std::string
 */
template <typename T>
using transparent_string_map = std::unordered_map<std::string, T, transparent_string_hash, transparent_string_equal>;

} // namespace warp::http
