#include "warp/http/server.hpp"
#include "../src/net/router/registry.hpp"

#include <boost/beast/http.hpp>
#include <boost/json/object.hpp>
#include <boost/json/array.hpp>
#include <boost/json/value.hpp>
#include <boost/json/parse.hpp>

#include <cassert>
#include <string>

#include "warp/db/postgres/connection_config.hpp"

int main() {
	warp::http::response res {boost::beast::http::status::ok, 11};
	res.body() = "ping";
	res.prepare_payload();
	assert(res.body() == "ping");

	warp::net::router::registry routes;
	routes.add("/hello/{id}", [](const warp::net::router::request &req) -> warp::net::router::response {
		warp::net::router::response response {boost::beast::http::status::ok, req.version()};
		response.body() = std::string(req.target());
		response.keep_alive(req.keep_alive());
		response.prepare_payload();
		return response;
	});

	auto match = routes.find("/hello/42?lang=en");
	assert(match);
	auto id_it = match->params.find("id");
	assert(id_it != match->params.end());
	assert(id_it->second == "42");

	warp::net::router::request req {boost::beast::http::verb::get, "/hello/42?lang=en", 11};
	req.keep_alive(true);
	req.body() = R"({"value":42})";

	auto matched_response = match->handler(req);
	assert(matched_response.body() == "/hello/42?lang=en");

	auto json_value = boost::json::parse(req.body());
	assert(json_value.at("value").as_int64() == 42);
	boost::json::object builder;
	builder["name"] = "warp";
	builder["answer"] = 42;
	boost::json::array numbers;
	numbers.push_back(1);
	numbers.push_back(2);
	builder["numbers"] = numbers;
	auto it = builder.find("numbers");
	assert(it != builder.end());
	const auto &numbers_array = it->value().as_array();
	assert(numbers_array.size() == 2);
	assert(numbers_array[0].as_int64() == 1);

	warp::net::router::request bad_json {boost::beast::http::verb::get, "/bad", 11};
	bad_json.body() = "not json";
	try {
		static_cast<void>(boost::json::parse(bad_json.body()));
		assert(false);
	} catch (const std::exception &) {
	}

	auto miss = routes.find("/goodbye/42");
	assert(!miss);

	warp::db::postgres::connection_config db_config;
	db_config.host = "db.internal";
	db_config.port = 5544;
	db_config.user = "user";
	db_config.password = "secret";
	db_config.database = "warp";
	db_config.extra_parameters = "application_name=warp";
	auto conninfo = db_config.to_connection_string();
	assert(conninfo.find("host=db.internal") != std::string::npos);
	assert(conninfo.find("port=5544") != std::string::npos);
	assert(conninfo.find("user=user") != std::string::npos);
	assert(conninfo.find("dbname=warp") != std::string::npos);

	return 0;
}
