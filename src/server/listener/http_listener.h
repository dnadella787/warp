//
// Created by Dhanush Nadella on 4/11/26.
//

#pragma once

#include <boost/asio/strand.hpp>

#include "base_listener.hpp"
#include "server/router/registry.hpp"
#include "warp/logging/logger.hpp"

namespace warp::server {

template <typename T>
concept Executor = requires(T t) {
    { t.execute() } -> std::same_as<void>;
};

template <typename T, typename... Args>
concept CanBeBuiltWith = requires(Args&&... args) {
        T(std::forward<Args>(args)...);
};

template <typename T>
concept WarpListener =
    Executor<T> &&
    CanBeBuiltWith<T, boost::asio::io_context&, registry&, std::string, unsigned short, log::logger>;

template<typename T>
class http_listener : public base_listener  {
public:
    http_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address, unsigned short port,
                  log::logger logger) requires WarpListener<T>;

    void run() override;

protected:
    boost::asio::io_context &ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    registry &registry_;
    log::logger logger_;
};

} // namespace warp::server

#include "http_listener.tpp"
