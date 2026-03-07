#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include "warp/net/http/request.hpp"
#include "warp/net/http/response.hpp"

namespace warp::http {

using request = net::http::request;
using response = net::http::response;
using headers = net::http::headers;
using method = net::http::method;
using handler = std::function<response(const request &)>;

class server;

class server_builder {
public:
    server_builder() = default;

    server_builder &address(std::string address);
    server_builder &port(std::uint16_t port);
    server_builder &worker_threads(std::size_t count);
    server_builder &route(std::string path, handler handler);

    [[nodiscard]] server build() const;

private:
    std::string address_{"0.0.0.0"};
    std::uint16_t port_{8080};
    std::size_t workers_{std::max<std::size_t>(1, std::thread::hardware_concurrency())};
    std::vector<std::pair<std::string, handler>> routes_;
};

class server {
    class impl;

public:
    server();
    ~server();
    server(server &&) noexcept;
    server &operator=(server &&) noexcept;

    void run();
    void stop();

    class controller {
    public:
	void stop();

    private:
	friend class server;
	explicit controller(std::shared_ptr<impl> impl);
	std::weak_ptr<impl> impl_;
    };

    [[nodiscard]] controller get_controller() const;
    [[nodiscard]] std::uint16_t port() const;

private:
    friend class server_builder;
    explicit server(std::shared_ptr<impl> impl);

    std::shared_ptr<impl> impl_;
};

} // namespace warp::http
