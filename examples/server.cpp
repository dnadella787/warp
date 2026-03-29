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
		               auto target = std::string_view(req.target());
		               auto slash = target.find_last_of('/');
		               auto name =
		                   slash == std::string_view::npos ? std::string_view {"world"} : target.substr(slash + 1);
		               auto resp = warp::http::response::ok("Hello, " + std::string(name) + "!", "text/plain");
		               resp.keep_alive(req.keep_alive());
		               return resp;
	               })
	        .route("/hello",
	               [](const warp::http::request &req) -> warp::http::response {
		               std::string_view target = req.target();
		               auto query_pos = target.find('?');
		               std::string_view name = "World";
		               if (query_pos != std::string_view::npos) {
			               auto query = target.substr(query_pos + 1);
			               constexpr std::string_view name_key = "name=";
			               auto name_pos = query.find(name_key);
			               if (name_pos != std::string_view::npos) {
				               name = query.substr(name_pos + name_key.size());
			               }
		               }
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
