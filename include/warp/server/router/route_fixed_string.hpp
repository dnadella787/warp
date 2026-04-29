#pragma once

#include <cstddef>
#include <string_view>

namespace warp::http {

// this is a compile time string wrapper that enables us to do things
// like warp::http::route_spec<warp::method::get, "/users/health">;
// b/c everything about "/users/health" is known at compile time (the constexpr)
template <std::size_t N>
struct fixed_string {
	char value[N] {};

	// the \0 byte at end of string
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

	// allows implicit conversion, ex:
	// fixed_string fs = "GET";
	// std::string_view v = fs;  <- implicit
	constexpr operator std::string_view() const noexcept {
		return view();
	}

	constexpr auto operator<=>(const fixed_string &) const = default;
};

// this helps the compiler deduce the length without needing to be explicitly provided
// otherwise we would need fixed_string<5> x = "POST" (the \0 byte at end of string).
// so instead we can write fixed_string x = "POST";
template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

} // namespace warp::http
