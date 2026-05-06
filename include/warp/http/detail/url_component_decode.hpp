#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace warp::http::detail {

inline int hex_value(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return 10 + (c - 'A');
	}
	if (c >= 'a' && c <= 'f') {
		return 10 + (c - 'a');
	}
	return -1;
}

// Percent-decoding is shared between path and query parsing, but '+' only means
// space for query-style decoding. Path segments keep '+' literal unless caller
// opts in with plus_as_space=true.
inline std::optional<std::string> try_decode_url_component(std::string_view input, bool plus_as_space = false) {
	std::string output;
	output.reserve(input.size());
	for (std::size_t i = 0; i < input.size(); ++i) {
		const char c = input[i];
		if (c == '%') {
			if (i + 2 >= input.size()) {
				return std::nullopt;
			}
			const int hi = hex_value(input[i + 1]);
			const int lo = hex_value(input[i + 2]);
			if (hi < 0 || lo < 0) {
				return std::nullopt;
			}
			output.push_back(static_cast<char>((hi << 4) | lo));
			i += 2;
			continue;
		}
		if (c == '+' && plus_as_space) {
			output.push_back(' ');
			continue;
		}
		output.push_back(c);
	}
	return output;
}

inline std::optional<std::string> try_decode_query_component(std::string_view input) {
	return try_decode_url_component(input, true);
}

} // namespace warp::http::detail
