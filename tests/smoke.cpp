#include "warp/http/server.hpp"
#include "../include/warp/net/router/router.hpp"

#include <boost/json/object.hpp>
#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

#include <cassert>
#include <string>

int main() {
    auto res = warp::http::response::ok("ping");
    assert(res.body() == "ping");

    warp::net::router::registry routes;
    routes.add("/hello/{id}", [](const warp::net::http::request& req) -> warp::net::http::response {
        auto id = req.path_param("id");
        assert(id);
        assert(req.path() == "/hello/42");
        auto lang = req.query_param("lang");
        if (lang) {
            return warp::net::http::response::ok(std::string(*id) + ":" + std::string(*lang));
        }
        return warp::net::http::response::ok("id=" + std::string(*id));
    });

    auto match = routes.find("/hello/42");
    assert(match);

    warp::net::http::request req{warp::net::http::method::get, "/hello/42?lang=en", R"({"value":42})"};
    req.set_path("/hello/42");
    req.set_query_params({{"lang", "en"}});
    req.set_path_params(std::move(match->params));

    auto matched_response = match->handler(req);
    assert(matched_response.body() == "42:en");
    auto id_view = req.path_param("id");
    assert(id_view && *id_view == "42");
    auto lang_view = req.query_param("lang");
    assert(lang_view && *lang_view == "en");

    auto json_value = req.json_body();
    assert(json_value.at("value").as_int64() == 42);
    auto try_json = req.try_json_body();
    assert(try_json);
    boost::json::object builder;
    builder["name"] = "warp";
    builder["answer"] = 42;
    boost::json::array numbers;
    numbers.push_back(1);
    numbers.push_back(2);
    builder["numbers"] = numbers;
    auto it = builder.find("numbers");
    assert(it != builder.end());
    const auto& numbers_array = it->value().as_array();
    assert(numbers_array.size() == 2);
    assert(numbers_array[0].as_int64() == 1);

    warp::net::http::request bad_json{warp::net::http::method::get, "/bad", "not json"};
    assert(!bad_json.try_json_body());

    auto miss = routes.find("/goodbye/42");
    assert(!miss);

    return 0;
}
