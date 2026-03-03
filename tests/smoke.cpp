#include "warp/http/server.hpp"

#include <cassert>

int main() {
    auto res = warp::http::response::ok("ping");
    assert(res.body() == "ping");
    return 0;
}
