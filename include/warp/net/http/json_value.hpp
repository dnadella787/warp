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
    explicit operator bool() const noexcept { return valid(); }

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

    static json_value parse(std::string_view text);
    static json_value object();
    static json_value array();
    static json_value from_bool(bool value);
    static json_value from_int64(std::int64_t value);
    static json_value from_uint64(std::uint64_t value);
    static json_value from_double(double value);
    static json_value from_string(std::string value);

    void set(std::string_view key, json_value value);
    void set(std::string_view key, bool value);
    void set(std::string_view key, std::int64_t value);
    void set(std::string_view key, std::uint64_t value);
    void set(std::string_view key, double value);
    void set(std::string_view key, std::string value);

    void push(json_value value);
    void push(bool value);
    void push(std::int64_t value);
    void push(std::uint64_t value);
    void push(double value);
    void push(std::string value);

private:
    struct state;
    std::shared_ptr<state> state_;

    explicit json_value(std::shared_ptr<state> state) noexcept;
    [[nodiscard]] state& require_state_mut();
    [[nodiscard]] const state& require_state() const;

    friend class request;
};

} // namespace warp::net::http
