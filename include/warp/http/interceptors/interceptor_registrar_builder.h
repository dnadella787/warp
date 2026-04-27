//
// Created by Dhanush Nadella on 4/27/26.
//
#pragma once

#include <tuple>

#include "interceptor_registrar.h"
#include "request_interceptor.h"

namespace warp::http {

template <int Priority, typename Interceptor, typename... StoredArgs>
struct interceptor_entry {
    static constexpr int priority = Priority;
    using interceptor_type = Interceptor;

    std::tuple<StoredArgs...> args;

    constexpr explicit interceptor_entry(StoredArgs... xs)
        : args(std::move(xs)...) {}
};

template <typename... Entries>
consteval auto sorted_indices_by_priority() {
    std::array<std::size_t, sizeof...(Entries)> indices{};

    for (std::size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    constexpr std::array<int, sizeof...(Entries)> priorities{
        Entries::priority...
    };

    std::sort(indices.begin(), indices.end(), [&](auto lhs, auto rhs) {
        return priorities[lhs] < priorities[rhs];
    });

    return indices;
}

template <auto Indices>
struct index_array_to_sequence;

template <std::size_t... Is>
struct index_array_to_sequence<std::array<std::size_t, sizeof...(Is)>{Is...}> {
    using type = std::index_sequence<Is...>;
};

template <typename EntriesTuple, auto SortedIndices, std::size_t... I>
constexpr auto make_registrar_impl(EntriesTuple entries, std::index_sequence<I...>) {
    return interceptor_registrar<
        EntriesTuple,
        SortedIndices[I]...
    >{std::move(entries)};
}

template <typename... Entries>
class interceptor_registrar_builder {
public:
    constexpr interceptor_registrar_builder() = default;

    constexpr explicit interceptor_registrar_builder(std::tuple<Entries...> entries)
        : entries_(std::move(entries))
    {}

    template <int Priority, typename Interceptor, typename... Args>
    constexpr auto interceptor(Args&&... args) const & {
        using entry_t = interceptor_entry<
            Priority,
            Interceptor,
            std::decay_t<Args>...
        >;

        return interceptor_registrar_builder<Entries..., entry_t>{
            std::tuple_cat(
                entries_,
                std::make_tuple(entry_t{std::forward<Args>(args)...})
            )
        };
    }

    constexpr auto build() const & {
        constexpr auto sorted = sorted_indices_by_priority<Entries...>();

        return make_registrar_impl<
            std::tuple<Entries...>,
            sorted
        >(
            entries_,
            std::make_index_sequence<sizeof...(Entries)>{}
        );
    }

private:
    std::tuple<Entries...> entries_;
};

}
