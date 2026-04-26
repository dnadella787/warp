//
// Created by Dhanush Nadella on 4/25/26.
//
#pragma once
#include "warp/db/postgres/connection_config.hpp"

namespace example {

[[nodiscard]] warp::db::postgres::connection_config make_db_config() {
	warp::db::postgres::connection_config config = {};
	if (const char *host = std::getenv("WARP_DB_HOST")) {
		config.host = host;
	}
	if (const char *port = std::getenv("WARP_DB_PORT")) {
		config.port = static_cast<std::uint16_t>(std::stoi(port));
	}
	if (const char *user = std::getenv("WARP_DB_USER")) {
		config.user = user;
	}
	if (const char *password = std::getenv("WARP_DB_PASSWORD")) {
		config.password = password;
	}
	if (const char *database = std::getenv("WARP_DB_NAME")) {
		config.database = database;
	}
	return config;
}

} // namespace example
