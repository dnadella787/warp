#include "warp/server/server.hpp"
#include "warp/db/postgres/connection_config.hpp"
#include "warp/db/postgres/connection_pool.hpp"

#include <memory>
#include <string>
#include <string_view>

#include "warp/warp.hpp"
#include "warp/server/router/route_spec.hpp"
#include "warp/server/server_builder.hpp"

#include "helpers.cpp"
#include "warp/ssl/file_cert_loader.hpp"

namespace {

bool env_flag_enabled(const char *name) {
	const char *raw = std::getenv(name);
	if (raw == nullptr) {
		return false;
	}

	const std::string_view value(raw);
	return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on";
}

std::string env_or_default(const char *name, std::string_view fallback) {
	if (const char *raw = std::getenv(name)) {
		return raw;
	}
	return std::string(fallback);
}

} // namespace

int main() {
	const bool tls_enabled = env_flag_enabled("WARP_EXAMPLE_TLS");
	const std::string tls_pem_bundle = env_or_default("WARP_EXAMPLE_TLS_PEM", "examples/tls/localhost.bundle.pem");
	const std::string_view scheme = tls_enabled ? "https" : "http";

	auto app_logger = warp::log::logger::stderr_color("warp.example");
	app_logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
	app_logger.set_level(warp::log::level::info);
	app_logger.set_as_default();

	auto db_pool = std::make_shared<warp::db::postgres::connection_pool>(boost::asio::system_executor {},
	                                                                     example::make_db_config(), 4, 2);
	auto interceptor = example::log_interceptor {};
	auto resp_interceptor = example::response_log_interceptor {};
	auto authz_interceptor = example::authz_interceptor {"Bob"};
	auto server =
	    warp::server::server_builder()
	        .address("127.0.0.1")
	        .worker_threads(4)
	        .port(8080)
	        // build() captures this logger for Warp's internal listener/session/server logs.
	        .logger(app_logger)
	        .interceptor<1>(interceptor)
	        // .interceptor<2>(authz_interceptor)
	        .interceptor<1>(resp_interceptor)
	        .ssl_config(tls_enabled ? warp::ssl::ssl_config(true, warp::ssl::file_cert_loader(tls_pem_bundle))
	                                : warp::ssl::ssl_config {})
	        .get<"/hello/{name}">([](const warp::request &req) -> warp::http::response {
		        auto name = req.path_param("name").value_or("world");
		        warp::log::info("Received a hello world request with name {}", name);
		        auto resp = warp::http::response::ok("Hello, " + std::string(name) + "!", "text/plain");
		        return resp;
	        })
	        .get<"/hello">([](const warp::http::request req) -> warp::http::response {
		        warp::log::info("Received a hello world request with no name query parameter");
		        auto resp = warp::response::ok(warp::body_builder().set("name", nullptr).build());
		        return resp;
	        })
	        .get<"/hello", warp::http::required_query<"name">>(
	            [](const warp::http::request req) -> warp::http::response {
		            auto name = req.query_param("name").value_or("World");
		            warp::log::info("Received a hello world request with required query parameter name={}", name);
		            auto resp = warp::response::ok(warp::body_builder().set("name", std::string(name)).build());
		            return resp;
	            })
	        .get<"/ping">([scheme](const warp::http::request) -> warp::http::response {
		        warp::log::info("ping request");
		        auto b = warp::body_builder().set("path", "ping").set("protocol", std::string(scheme));
		        auto resp = warp::response::ok(warp::body_builder().set("endpoint", b.json()).build());
		        return resp;
	        })
	        .get<"/db/{id}">([db_pool](warp::request req) -> warp::awaitable<warp::response> {
		        auto id = req.path_param("id").value_or("");

		        auto result =
		            co_await db_pool->query(std::format("SELECT * FROM exchanges WHERE exchange_code = '{}';", id));
		        if (result.rows() == 0)
			        co_return warp::response::not_found(std::format("No exchange with code={} found", id));

		        if (result.rows() > 1)
			        // end user gets 500 error
			        throw std::runtime_error(std::format("Multiple exchanges for the same code={}", id));

		        co_return warp::response::ok(warp::body_builder().set("exchange_name", result.value(0, 1)).build());
	        })
	        .build<warp::event_loop_mode::callbacks>();
	warp::log::info("Warp example server running on {}://127.0.0.1:8080", scheme);
	warp::log::info("Set WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME to try GET /db/{{id}}");
	warp::log::info("Changing the default logger after build() does not retarget this server.");
	if (tls_enabled) {
		warp::log::info("TLS enabled with PEM bundle at {}", tls_pem_bundle);
		warp::log::info("Try: curl --cacert examples/tls/localhost-ca.pem 'https://localhost:8080/hello?name=Bob'");
	} else {
		warp::log::info(
		    "Set WARP_EXAMPLE_TLS=1 and WARP_EXAMPLE_TLS_PEM=examples/tls/localhost.bundle.pem to enable TLS");
	}
	server.run();
	return 0;
}
