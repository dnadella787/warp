#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace warp::net::http {

class json_value {
public:
    json_value();
    json_value(const json_value&);
    json_value(json_value&&) noexcept;
    json_value& operator=(const json_value&);
    json_value& operator=(json_value&&) noexcept;
    ~json_value();

    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] bool is_null() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_int64() const;
    [[nodiscard]] bool is_uint64() const;
    [[nodiscard]] bool is_double() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_object() const;

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] std::int64_t as_int64() const;
    [[nodiscard]] std::uint64_t as_uint64() const;
    [[nodiscard]] double as_double() const;
    [[nodiscard]] std::string as_string() const;

    [[nodiscard]] json_value at(std::string_view key) const;
    [[nodiscard]] json_value get(std::size_t index) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::string dump() const;

    [[nodiscard]] static json_value parse(std::string_view text);

private:
    struct state;
    std::shared_ptr<state> state_;

    explicit json_value(std::shared_ptr<state> state) noexcept;
    [[nodiscard]] const state& require_state() const;

    friend class request;
};

} // namespace warp::net::http
