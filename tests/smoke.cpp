#include "warp/http/server.hpp"
#include "warp/net/router.hpp"

#include <cassert>
#include <string>

int main() {
    auto res = warp::http::response::ok("ping");
    assert(res.body() == "ping");

    warp::net::router::registry routes;
    routes.add("/hello/{id}", [](const warp::net::http::request& req) -> warp::net::http::response {
        auto id = req.path_param("id");
        assert(id);
        return warp::net::http::response::ok("id=" + std::string(*id));
    });

    auto match = routes.find("/hello/42");
    assert(match);

    warp::net::http::request req{warp::net::http::method::get, "/hello/42", ""};
    req.set_path_params(std::move(match->params));

    auto matched_response = match->handler(req);
    assert(matched_response.body() == "id=42");
    auto id_view = req.path_param("id");
    assert(id_view && *id_view == "42");

    auto miss = routes.find("/goodbye/42");
    assert(!miss);

    return 0;
}
