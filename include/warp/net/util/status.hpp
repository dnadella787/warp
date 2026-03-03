#pragma once

#include <string>
#include <system_error>

namespace warp::net::util {

enum class status_code {
    ok = 0,
    cancelled,
    network_error,
    protocol_error,
    timeout,
    internal_error
};

class status_category : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::string message(int ev) const override;
};

const std::error_category& status_category_instance();
std::error_code make_error(status_code code);

struct error_info {
    std::error_code code{};
    std::string message{};
};

} // namespace warp::net::util
