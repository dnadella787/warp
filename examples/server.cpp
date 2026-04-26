#include "warp/server/server.hpp"
#include "warp/db/postgres/connection_config.hpp"
#include "warp/db/postgres/connection_pool.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "warp/warp.hpp"
#include "warp/server/router/route_spec.hpp"
#include "warp/server/server_builder.hpp"

#include "helpers.cpp"

int main() {
	auto db_pool = std::make_shared<warp::db::postgres::connection_pool>(boost::asio::system_executor {},
	                                                                     example::make_db_config(), 4, 2);

	auto server =
	    warp::server::server_builder()
	        .address("127.0.0.1")
	        .worker_threads(4)
	        .port(8080)
	        .get("/hello/{name}",
	             [](const warp::request &req) -> warp::http::response {
		             auto name = req.path_param("name").value_or("world");
		             auto resp = warp::http::response::ok("Hello, " + std::string(name) + "!", "text/plain");
		             return resp;
	             })
	        .get("/hello",
	             [](const warp::http::request req) -> warp::http::response {
		             auto name = req.query_param("name").value_or("World");
		             std::cout << "Received a hello world request with query parameter name with value: " << name
		                       << std::endl;
		             auto resp = warp::response::ok(warp::body_builder().set("name", std::string(name)).build());
		             return resp;
	             })
	        .get("/ping",
	             [](const warp::http::request) -> warp::http::response {
		             std::cout << "ping request" << std::endl;
		             auto b = warp::body_builder().set("path", "ping").set("protocol", "http");
		             auto resp = warp::response::ok(warp::body_builder().set("endpoint", b.json()).build());
		             return resp;
	             })
	        .get(
	            "/db/{id}",
	            [db_pool](warp::request req) -> warp::awaitable<warp::response> {
		            auto id = req.path_param("id").value_or("");

		            auto result =
		                co_await db_pool->query(std::format("SELECT * FROM exchanges WHERE exchange_code = '{}';", id));
		            if (result.rows() == 0) {
			            co_return warp::response::not_found(std::format("No exchange with code={} found", id));
		            }
		            if (result.rows() > 1) {
			            // end user gets 500 error
			            throw std::runtime_error(std::format("Multiple exchanges for the same code={}", id));
		            }
		            co_return warp::response::ok(warp::body_builder().set("exchange_name", result.value(0, 1)).build());
	            })
	        .build<warp::event_loop_mode::coroutines>();
	std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
	std::cout << "Set WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME to try GET /db/{id}" << std::endl;
	server.run();
	return 0;
}
