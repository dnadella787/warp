#include "http_db_test_support.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <chrono>
#include <cstdlib>

namespace warp::tests::integration_support {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

warp::db::postgres::connection_config make_db_config(const db_env &env) {
	warp::db::postgres::connection_config config;
	config.host = env.host.empty() ? "127.0.0.1" : env.host;
	config.port = env.port;
	config.user = env.user;
	config.password = env.password;
	config.database = env.database;
	return config;
}

std::optional<db_env> load_db_env() {
	const char *user = std::getenv("WARP_DB_USER");
	const char *password = std::getenv("WARP_DB_PASSWORD");
	const char *database = std::getenv("WARP_DB_NAME");
	if (user == nullptr || password == nullptr || database == nullptr) {
		return std::nullopt;
	}

	db_env env;
	env.user = user;
	env.password = password;
	env.database = database;
	if (const char *host = std::getenv("WARP_DB_HOST")) {
		env.host = host;
	}
	if (const char *port = std::getenv("WARP_DB_PORT")) {
		env.port = static_cast<std::uint16_t>(std::stoi(port));
	}
	return env;
}

std::optional<std::string> probe_db_connection(const db_env &env) {
	asio::io_context ioc;
	beast::tcp_stream stream(ioc);
	stream.expires_after(std::chrono::seconds(1));

	const auto host = env.host.empty() ? std::string("127.0.0.1") : env.host;
	const auto port = std::to_string(env.port.value_or(5432));

	tcp::resolver resolver(ioc);
	beast::error_code ec;
	const auto endpoints = resolver.resolve(host, port, ec);
	if (ec) {
		return ec.message();
	}

	stream.connect(endpoints, ec);
	if (ec) {
		return ec.message();
	}

	stream.socket().shutdown(tcp::socket::shutdown_both, ec);
	return std::nullopt;
}

} // namespace warp::tests::integration_support
