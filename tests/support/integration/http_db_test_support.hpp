#pragma once

#include "warp/db/postgres/connection_config.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace warp::tests::integration_support {

struct db_env {
	std::string host;
	std::optional<std::uint16_t> port;
	std::string user;
	std::string password;
	std::string database;
};

warp::db::postgres::connection_config make_db_config(const db_env &env);
std::optional<db_env> load_db_env();
std::optional<std::string> probe_db_connection(const db_env &env);

} // namespace warp::tests::integration_support
