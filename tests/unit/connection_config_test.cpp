#include "warp/db/postgres/connection_config.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace {

using warp::db::postgres::connection_config;

TEST(ConnectionConfigTest, BuildsConnectionStringFromConfiguredFields) {
	connection_config config;
	config.host = "db.internal";
	config.port = 5544;
	config.user = "app_user";
	config.password = "secret";
	config.database = "warp_db";
	config.connect_timeout = std::chrono::seconds(7);
	config.extra_parameters = "application_name=warp sslmode=disable";

	auto conninfo = config.to_connection_string();

	EXPECT_NE(conninfo.find("host=db.internal"), std::string::npos);
	EXPECT_NE(conninfo.find("port=5544"), std::string::npos);
	EXPECT_NE(conninfo.find("user=app_user"), std::string::npos);
	EXPECT_NE(conninfo.find("password=secret"), std::string::npos);
	EXPECT_NE(conninfo.find("dbname=warp_db"), std::string::npos);
	EXPECT_NE(conninfo.find("connect_timeout=7"), std::string::npos);
	EXPECT_NE(conninfo.find("application_name=warp sslmode=disable"), std::string::npos);
}

TEST(ConnectionConfigTest, EscapesSpecialCharactersInTextFields) {
	connection_config config;
	config.host = R"(db\primary)";
	config.user = R"(warp"user)";
	config.password = R"(pa'ss)";
	config.database = R"(main db)";

	auto conninfo = config.to_connection_string();

	EXPECT_NE(conninfo.find(R"(host=db\\primary)"), std::string::npos);
	EXPECT_NE(conninfo.find(R"(user=warp\"user)"), std::string::npos);
	EXPECT_NE(conninfo.find(R"(password=pa\'ss)"), std::string::npos);
	EXPECT_NE(conninfo.find("dbname=main db"), std::string::npos);
}

TEST(ConnectionConfigTest, OmitsUnsetOptionalFields) {
	connection_config config;
	config.host.clear();
	config.port = std::nullopt;
	config.user.clear();
	config.password.clear();
	config.database.clear();
	config.connect_timeout = std::nullopt;
	config.extra_parameters.clear();

	EXPECT_EQ(config.to_connection_string(), "");
}

} // namespace
