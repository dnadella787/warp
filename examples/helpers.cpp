//
// Created by Dhanush Nadella on 4/25/26.
//
#pragma once
#include "warp/db/postgres/connection_config.hpp"
#include "warp/warp.hpp"

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

struct log_interceptor {
	log_interceptor() = default;

	static void intercept(warp::request &request) {
		warp::log::info("Entering request log interceptor");

		auto maybe_name = request.path_param("name");
		if (!maybe_name) {
			warp::log::info("Name path param not available for this request");
			return;
		}

		warp::log::info("Name path param passed: {}", maybe_name.value());
	}
};

class authz_interceptor {
public:
	authz_interceptor() = delete;
	authz_interceptor(std::string_view allowed_name) : allowed_name_(allowed_name) {
	}

	std::optional<warp::response> intercept(warp::request &request) const {
		warp::log::info("Entering request authz interceptor");

		auto maybe_name = request.query_param("name");
		if (!maybe_name) {
			warp::log::info("Name path param not available for this request, rejecting with 404");
			return warp::response::not_found();
		}

		if (maybe_name.value() != allowed_name_) {
			warp::log::info("Name {} not authorized for API requests, rejecting with 404", maybe_name.value());
			return warp::response::not_found();
		}

		return std::nullopt;
	}

private:
	std::string_view allowed_name_;
};

} // namespace example
