#include "warp/http/server.hpp"

#include <iostream>
#include <string>
#include <string_view>

int main() {
    auto server = warp::http::server_builder()
                       .address("127.0.0.1")
                       .worker_threads(4)
                       .port(8080)
                       .route("/hello/{name}", [](const warp::http::request& req) -> warp::http::response {
                           auto name = req.path_param("name").value_or("world");
                           return warp::http::response::ok("Hello, " + std::string(name) + "!");
                       })
                       .route("/hello", [](const warp::http::request& req) -> warp::http::response {
                           auto name = req.query_param("name").value_or("World");
                           std::cout << "Received a hello world request with query parameter name with value: " << name << std::endl;
                           auto json = warp::net::http::json_value::object();
                           json.set("name", std::string(name));
                           return warp::http::response::ok(json.dump());
                       })
                       .build();

    std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
    server.run();
    return 0;
}
