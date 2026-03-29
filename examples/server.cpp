#include "warp/http/server.hpp"

#include <iostream>
#include <string>
#include <string_view>

int main() {
	auto server =
	    warp::http::server_builder()
	        .address("127.0.0.1")
	        .worker_threads(4)
	        .port(8080)
	        .route("/hello/{name}",
	               [](const warp::request &req) -> warp::http::response {
		               auto name = req.path_param("name").value_or("world");
		               auto resp = warp::http::response::ok("Hello, " + std::string(name) + "!", "text/plain");
		               resp.keep_alive(req.keep_alive());
		               return resp;
	               })
	        .route("/hello",
	               [](const warp::http::request &req) -> warp::http::response {
		               auto name = req.query_param("name").value_or("World");
		               std::cout << "Received a hello world request with query parameter name with value: " << name
		                         << std::endl;
		               auto resp = warp::response::ok(warp::body_builder().set("name", std::string(name)).build());
		               resp.keep_alive(req.keep_alive());
		               return resp;
	               })
	        .build();
	std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
	server.run();
	return 0;
}
