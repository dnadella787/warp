#include "warp/http/server.hpp"
#include "../src/http/registry.hpp"

#include <boost/json/object.hpp>
#include <boost/json/array.hpp>
#include <boost/json/value.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/string.hpp>

#include <cassert>
#include <string>

#include "warp/db/postgres/connection_config.hpp"

int main() {
	auto res = warp::http::response::ok("ping");
	assert(res.body() == "ping");
	assert(res[boost::beast::http::field::content_type] == "application/json");

	auto built_body = warp::body_builder().set("name", "client").set("second", "bob").set("count", 2).build();
	auto built_json = boost::json::parse(built_body).as_object();
	assert(built_json.at("name").as_string() == "client");
	assert(built_json.at("second").as_string() == "bob");
	assert(built_json.at("count").as_int64() == 2);

	auto built_response = warp::response_builder().status(200).body(warp::body_builder().set("x", "y").build()).build();
	assert(built_response.result() == boost::beast::http::status::ok);
	assert(built_response[boost::beast::http::field::content_type] == "application/json");
	auto built_response_json = boost::json::parse(built_response.body()).as_object();
	assert(built_response_json.at("x").as_string() == "y");

	boost::json::object ok_payload;
	ok_payload["name"] = "warp";
	ok_payload["ready"] = true;
	auto json_response = warp::http::response::ok(boost::json::value(ok_payload));
	assert(json_response[boost::beast::http::field::content_type] == "application/json");
	auto parsed_ok_payload = boost::json::parse(json_response.body()).as_object();
	assert(parsed_ok_payload.at("name").as_string() == "warp");
	assert(parsed_ok_payload.at("ready").as_bool());

	auto not_found = warp::http::response::not_found("missing");
	assert(not_found[boost::beast::http::field::content_type] == "application/json");
	auto not_found_json = boost::json::parse(not_found.body()).as_object();
	assert(not_found_json.at("error").as_string() == "missing");

	warp::http::registry routes;
	routes.add("/hello/{id}", [](const warp::request &req) -> warp::response {
		auto id = req.path_param("id");
		auto lang = req.query_param("lang");
		assert(id);
		assert(lang);
		assert(req.path() == "/hello/42");
		assert(req.path_params().at("id") == "42");
		assert(req.query_params().at("lang") == "en");
		auto response = warp::http::response::ok(
		    warp::body_builder().set("id", std::string(*id)).set("lang", std::string(*lang)).build());
		response.keep_alive(req.keep_alive());
		return response;
	});

	auto handler = routes.find("/hello/42?lang=en");
	assert(handler);

	warp::request req {boost::beast::http::verb::get, "/hello/42?lang=en", 11};
	req.keep_alive(true);
	req.body() = R"({"value":42})";

	auto matched_response = (*handler)(req);
	auto matched_response_json = boost::json::parse(matched_response.body()).as_object();
	assert(matched_response_json.at("id").as_string() == "42");
	assert(matched_response_json.at("lang").as_string() == "en");

	auto json_value = boost::json::parse(req.body());
	assert(json_value.at("value").as_int64() == 42);
	assert(req.path() == "/hello/42");
	assert(req.query_param("lang").value_or("") == "en");
	assert(!req.path_param("id"));
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

	warp::request bad_json {boost::beast::http::verb::get, "/bad", 11};
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
