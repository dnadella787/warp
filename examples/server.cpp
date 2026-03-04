#include "warp/http/server.hpp"

#include <iostream>

int main() {
    auto server = warp::http::server_builder()
                       .address("127.0.0.1")
                       .worker_threads(4)
                       .port(8080)
                       .route("/hello", [](const warp::http::request& req) -> warp::http::response {
                           (void)req;
                           return warp::http::response::ok("Hello, World!");
                       })
                       .build();

    std::cout << "Warp example server running on http://127.0.0.1:8080" << std::endl;
    server.run();
    return 0;
}
