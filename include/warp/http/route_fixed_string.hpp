#pragma once

#include <compare>
#include <cstddef>
#include <string_view>

namespace warp::http {

template <std::size_t N>
struct fixed_string {
	char value[N] {};

	constexpr fixed_string(const char (&literal)[N]) {
		for (std::size_t i = 0; i < N; ++i) {
			value[i] = literal[i];
		}
	}

	[[nodiscard]] constexpr std::size_t size() const noexcept {
		return N > 0 ? N - 1 : 0;
	}

	[[nodiscard]] constexpr std::string_view view() const noexcept {
		return {value, size()};
	}

	constexpr operator std::string_view() const noexcept {
		return view();
	}

	constexpr auto operator<=>(const fixed_string &) const = default;
};

template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

} // namespace warp::http
