#include "warp/http/server.hpp"

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <iostream>
#include <string>
#include <string_view>

#include "../../../../../opt/homebrew/Cellar/boost/1.90.0_1/include/boost/asio/io_context.hpp"

int main() {
	auto server = warp::http::server_builder()
	                  .address("127.0.0.1")
	                  .worker_threads(4)
	                  .port(8080)
	                  .route("/hello/{name}",
	                         [](const warp::http::request &req) -> warp::http::response {
		                         auto name = req.path_param("name").value_or("world");
		                         return warp::http::response::ok("Hello, " + std::string(name) + "!");
	                         })
	                  .route("/hello",
	                         [](const warp::http::request &req) -> warp::http::response {
		                         auto name = req.query_param("name").value_or("World");
		                         std::cout
		                             << "Received a hello world request with query parameter name with value: " << name
		                             << std::endl;
		                         boost::json::object json;
		                         json["name"] = name;
		                         return warp::http::response::ok(boost::json::serialize(json));
	                         })
	                  .build();
	boost::asio::io_context ioc {3};
	std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
	server.run();
	return 0;
}
