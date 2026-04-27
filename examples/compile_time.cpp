//
// Created by Dhanush Nadella on 4/25/26.
//

#include <iostream>

#include "warp/warp.hpp"
#include "warp/db/postgres/connection_pool.hpp"
#include "warp/server/server_builder.hpp"

#include <memory>
#include "helpers.cpp"

class exchange_resource {
public:
	exchange_resource() {
		db_pool = std::make_shared<warp::db::postgres::connection_pool>(boost::asio::system_executor {},
		                                                                example::make_db_config(), 4, 2);
	}

	warp::awaitable<warp::response> get_exchange(const warp::request &request) const {
		auto id = request.path_param("id").value_or("");
		auto result = co_await db_pool->query(std::format("SELECT * FROM exchanges WHERE exchange_code = '{}';", id));
		if (result.rows() == 0)
			co_return warp::response::not_found(std::format("No exchange with code={} found", id));
		co_return warp::response::ok(warp::body_builder().set("exchange_name", result.value(0, 1)).build());
	}

	warp::response get_sync(const warp::request &request) const {
		auto status = request.query_param("status").value_or("");
		return warp::response_builder()
		    .body(warp::body_builder().set("is-running", false).set("status", status).build())
		    .build();
	}

	warp::response get_sync_running(const warp::request &request) const {
		auto status = request.query_param("status").value_or("");
		return warp::response_builder()
		    .body(warp::body_builder().set("is-running", true).set("status", status).build())
		    .build();
	}

private:
	std::shared_ptr<warp::db::postgres::connection_pool> db_pool;
};

int main() {
	using namespace warp::http;
	using warp::server::server_builder;
	exchange_resource exchange_resource;

	auto server =
	    server_builder()
	        .address("127.0.0.1")
	        .port(8080)
	        .worker_threads(4)
	        // bc the coroutine takes over the ownership of the request using std::move for the duration
	        // of its execution (there is no unnecessary heap allocations)
	        .get<"/db/{id}">([&er = exchange_resource](warp::request request) -> warp::awaitable<response> {
		        co_return co_await er.get_exchange(request);
	        })
	        // will give you compile time error
	        // .get<"//", required_query<"status">>(
	        //     [&er = exchange_resource](const warp::request &request) -> response { return er.get_sync(request); })
	        // required query param
	        .get<"/sync", required_query<"status">>(
	            [&er = exchange_resource](const warp::request &request) -> response { return er.get_sync(request); })
	        // optional query param
	        .get<"/sync", optional_query_value<"status", "RUNNING">>(
	            [&er = exchange_resource](const warp::request &request) -> response {
		            return er.get_sync_running(request);
	            })
	        // requires /req?status=param /req -> 404
	        .get<"/req", required_query_value<"status", "param">>(
	            [&er = exchange_resource](const warp::request &request) -> response {
		            return er.get_sync_running(request);
	            })
	        .build();

	std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
	std::cout << "Set WARP_DB_USER / WARP_DB_PASSWORD / WARP_DB_NAME to try GET /db/{id}" << std::endl;
	server.run(true);
	std::cerr << "error server stopped running" << std::endl;
}
