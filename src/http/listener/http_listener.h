//
// Created by Dhanush Nadella on 4/11/26.
//

#pragma once
#include "base_listener.hpp"
#include "../../common/util/fail.h"
#include "boost/asio/strand.hpp"
#include "../router/registry.hpp"

namespace warp::http {

template <typename T>
concept Executor = requires(T t) {
    { t.execute() } -> std::same_as<void>;
};

template<typename T>
class http_listener : public base_listener  {
public:
    http_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address, unsigned short port) requires Executor<T>;

    void run() override;

protected:
    boost::asio::io_context &ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    registry &registry_;
};

}

#include "http_listener.tpp"

