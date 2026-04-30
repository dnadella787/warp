//
// Created by Dhanush Nadella on 4/11/26.
//

#pragma once

#include <concepts>
#include <utility>

#include <boost/asio/strand.hpp>

#include "server/execution/route_executor_table.hpp"
#include "base_listener.hpp"
#include "server/interceptors/interceptor_chain.h"
#include "server/router/registry.hpp"
#include "warp/logging/logger.hpp"

namespace warp::server {

template <typename T>
concept listener_executor = requires(T t) {
    { t.execute() } -> std::same_as<void>;
};

template <typename T, typename... Args>
concept can_be_built_with = requires(Args&&... args) {
        T(std::forward<Args>(args)...);
};

template <typename T, typename RouteExecutors>
concept warp_listener =
    listener_executor<T> &&
    can_be_built_with<T, boost::asio::io_context&, const registry&, const RouteExecutors&,
                   const interceptor_chain<request>&, const interceptor_chain<response>&, std::string, unsigned short,
                   log::logger>;

template<typename T, typename RouteExecutors>
class http_listener : public base_listener  {
public:
    http_listener(boost::asio::io_context &ioc, const registry &registry, const RouteExecutors &route_executors,
                  const interceptor_chain<request> &req_interceptor_chain,
                  const interceptor_chain<response> &resp_interceptor_chain, const std::string &address,
                  unsigned short port, log::logger logger) requires warp_listener<T, RouteExecutors>;

    void run() override;

protected:
    boost::asio::io_context &ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    const registry &registry_;
    const RouteExecutors &route_executors_;
    const interceptor_chain<request> &req_interceptor_chain_;
    const interceptor_chain<response> &resp_interceptor_chain_;
    log::logger logger_;
};

} // namespace warp::server

#include "http_listener.tpp"
