#include "warp/db/postgres/connection_config.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace warp::db::postgres {

namespace {

std::string escape(std::string_view input) {
	std::string out;
	out.reserve(input.size());
	for (char c : input) {
		if (std::isspace(static_cast<unsigned char>(c))) {
			out.push_back(' ');
			continue;
		}
		if (c == '\\' || c == '\'' || c == '"') {
			out.push_back('\\');
		}
		out.push_back(c);
	}
	return out;
}

} // namespace

std::string connection_config::to_connection_string() const {
	std::string conninfo;
	conninfo.reserve(128);
	if (!host.empty()) {
		conninfo += "host=" + escape(host) + ' ';
	}
	if (port) {
		conninfo += "port=" + std::to_string(*port) + ' ';
	}
	if (!user.empty()) {
		conninfo += "user=" + escape(user) + ' ';
	}
	if (!password.empty()) {
		conninfo += "password=" + escape(password) + ' ';
	}
	if (!database.empty()) {
		conninfo += "dbname=" + escape(database) + ' ';
	}
	if (connect_timeout) {
		conninfo += "connect_timeout=" + std::to_string(connect_timeout->count()) + ' ';
	}
	if (!extra_parameters.empty()) {
		conninfo += extra_parameters;
	}
	return conninfo;
}

} // namespace warp::db::postgres
