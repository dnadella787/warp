#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "warp/http/request.hpp"
#include "warp/http/response.hpp"

namespace warp::http {

using request = request;
using response = response;
using handler = std::function<response(const request &)>;

class registry {
public:
	registry() = default;
	registry(const registry &other);
	registry &operator=(const registry &other);
	void add(std::string path, handler h);
	[[nodiscard]] std::optional<handler> find(std::string_view path) const;

private:
	struct segment {
		enum class kind {
			literal,
			parameter
		};
		kind type {};
		std::string value;
	};

	struct route_entry {
		std::string pattern;
		std::vector<segment> segments;
		handler handler;
	};

	static std::vector<segment> compile_pattern(const std::string &pattern);
	static bool match_segments(const std::vector<segment> &pattern_segments,
	                           const std::vector<std::string_view> &path_segments,
	                           std::unordered_map<std::string, std::string> &out_params);

	[[nodiscard]] static std::vector<std::string_view> split_path(std::string_view path);

	std::vector<route_entry> routes_;
};

} // namespace warp::http
