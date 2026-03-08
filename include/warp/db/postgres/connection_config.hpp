#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace warp::db::postgres {

class connection_config {
public:
	std::string host {"127.0.0.1"};
	std::optional<std::uint16_t> port {5432};
	std::string user;
	std::string password;
	std::string database;
	std::optional<std::chrono::seconds> connect_timeout {};
	std::string extra_parameters;

	[[nodiscard]] std::string to_connection_string() const;
};

} // namespace warp::db::postgres
