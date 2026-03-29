#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/beast/http.hpp>

#include "warp/http/body_builder.hpp"
#include "warp/http/request.hpp"
#include "warp/http/response.hpp"
#include "warp/http/response_builder.hpp"

namespace warp::http {

using headers = request::fields_type;
using method = boost::beast::http::verb;
using handler = std::function<response(const request &)>;

class server;

class server_builder {
public:
	server_builder() = default;

	server_builder &address(std::string address);
	server_builder &port(std::uint16_t port);
	server_builder &worker_threads(std::size_t count);
	server_builder &route(method verb, std::string path, handler handler);
	server_builder &route(std::string path, handler handler);
	server_builder &get(std::string path, handler handler);
	server_builder &post(std::string path, handler handler);
	server_builder &put(std::string path, handler handler);
	server_builder &delete_(std::string path, handler handler);

	[[nodiscard]] server build() const;

private:
	struct route_definition {
		method verb;
		std::string path;
		handler callback;
	};

	std::string address_ {"0.0.0.0"};
	std::uint16_t port_ {8080};
	std::size_t workers_ {std::max<std::size_t>(1, std::thread::hardware_concurrency())};
	std::vector<route_definition> routes_;
};

class server {
	class impl;

public:
	server();
	~server();
	server(server &&) noexcept;
	server &operator=(server &&) noexcept;

	void run(bool blocking = true);
	void stop();

	// note that controller keeps a reference to the impl and not the server
	// object itself since server is a stack allocated obj, we do not keep any shared_ptr
	// references to it anywhere. This way we can entirely decouple the lifetimes of the
	// server and controller objects
	class controller {
	public:
		void stop();

	private:
		friend class server;
		explicit controller(const std::shared_ptr<impl> &impl);
		std::weak_ptr<impl> impl_;
	};

	[[nodiscard]] controller get_controller() const;

private:
	friend class server_builder;
	explicit server(std::shared_ptr<impl> impl);

	// we use std::shared_ptr instead of std::unique_ptr so controller can get a std::weak_ptr to impl
	std::shared_ptr<impl> impl_;
};

} // namespace warp::http

namespace warp {

using body_builder = http::body_builder;
using response_builder = http::response_builder;
using request = http::request;
using response = http::response;
using headers = http::headers;
using method = http::method;
using handler = http::handler;

} // namespace warp
