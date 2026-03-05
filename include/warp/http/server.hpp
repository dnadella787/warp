#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "warp/net/http.hpp"
#include "warp/net/router.hpp"

namespace warp::http {

using request = warp::net::http::request;
using response = warp::net::http::response;
using headers = warp::net::http::headers;
using method = warp::net::http::method;
using handler = warp::net::router::handler;

class server;

class server_builder {
public:
    server_builder() = default;

    server_builder& address(std::string address);
    server_builder& port(std::uint16_t port);
    server_builder& worker_threads(std::size_t count);
    server_builder& route(std::string path, handler handler);

    [[nodiscard]] server build() const;

private:
    std::string address_{"0.0.0.0"};
    std::uint16_t port_{8080};
    std::size_t workers_{std::max<std::size_t>(1, std::thread::hardware_concurrency())};
    warp::net::router::registry routes_;
};

class server {
    class impl;

public:
    server();
    ~server();
    server(server&&) noexcept;
    server& operator=(server&&) noexcept;

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
