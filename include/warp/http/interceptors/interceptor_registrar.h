//
// Created by Dhanush Nadella on 4/27/26.
//

#pragma once
#include "warp/warp.hpp"

namespace warp::http {

template <typename EntriesTuple, std::size_t... Order>
class interceptor_registrar {
public:
    constexpr explicit interceptor_registrar(EntriesTuple entries)
        : entries_(std::move(entries))
    {}

    template <typename Fn>
    constexpr decltype(auto) for_each(Fn&& fn) & {
        return for_each_impl(*this, std::forward<Fn>(fn));
    }

    template <typename Fn>
    constexpr decltype(auto) for_each(Fn&& fn) const & {
        return for_each_impl(*this, std::forward<Fn>(fn));
    }

private:
    template <typename Self, typename Fn>
    static constexpr void for_each_impl(Self& self, Fn&& fn) {
        (std::forward<Fn>(fn)(std::get<Order>(self.entries_)), ...);
    }

    EntriesTuple entries_;
};

}
