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
                           auto name = req.path_param("name").value_or(std::string_view("world"));
                           return warp::http::response::ok("Hello, " + std::string(name) + "!");
                       })
                       .route("/hello", [](const warp::http::request& req) -> warp::http::response {
                           (void)req;
                           return warp::http::response::ok("Hello, World!");
                       })
                       .build();

    std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
    server.run();
    return 0;
}
