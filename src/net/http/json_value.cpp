#include "warp/net/http/json_value.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace warp::net::http {

struct json_value::state {
    std::shared_ptr<boost::json::value> root;
    const boost::json::value* node{nullptr};
};

json_value::json_value() = default;

json_value::json_value(std::shared_ptr<state> state) noexcept
    : state_(std::move(state)) {}

json_value::json_value(const json_value&) = default;

json_value::json_value(json_value&&) noexcept = default;

json_value& json_value::operator=(const json_value&) = default;

json_value& json_value::operator=(json_value&&) noexcept = default;

json_value::~json_value() = default;

bool json_value::valid() const noexcept {
    return state_ && state_->node != nullptr;
}

const json_value::state& json_value::require_state() const {
    if (!state_ || state_->node == nullptr) {
        throw std::runtime_error("json_value: no associated data");
    }
    return *state_;
}

bool json_value::is_null() const {
    return require_state().node->is_null();
}

bool json_value::is_bool() const {
    return require_state().node->is_bool();
}

bool json_value::is_int64() const {
    return require_state().node->is_int64();
}

bool json_value::is_uint64() const {
    return require_state().node->is_uint64();
}

bool json_value::is_double() const {
    return require_state().node->is_double();
}

bool json_value::is_string() const {
    return require_state().node->is_string();
}

bool json_value::is_array() const {
    return require_state().node->is_array();
}

bool json_value::is_object() const {
    return require_state().node->is_object();
}

bool json_value::as_bool() const {
    return require_state().node->as_bool();
}

std::int64_t json_value::as_int64() const {
    return require_state().node->as_int64();
}

std::uint64_t json_value::as_uint64() const {
    return require_state().node->as_uint64();
}

double json_value::as_double() const {
    return require_state().node->as_double();
}

std::string json_value::as_string() const {
    return std::string(require_state().node->as_string());
}

json_value json_value::at(std::string_view key) const {
    const auto& current = require_state();
    const auto& node = *current.node;
    if (!node.is_object()) {
        throw std::runtime_error("json_value::at requires an object");
    }
    const auto& obj = node.as_object();
    auto it = obj.find(key);
    if (it == obj.end()) {
        throw std::out_of_range("json_value::at key not found");
    }
    auto state = std::make_shared<json_value::state>();
    state->root = current.root;
    state->node = &it->value();
    return json_value(std::move(state));
}

json_value json_value::get(std::size_t index) const {
    const auto& current = require_state();
    const auto& node = *current.node;
    if (!node.is_array()) {
        throw std::runtime_error("json_value::get requires an array");
    }
    const auto& arr = node.as_array();
    if (index >= arr.size()) {
        throw std::out_of_range("json_value::get index out of range");
    }
    auto state = std::make_shared<json_value::state>();
    state->root = current.root;
    state->node = &arr[index];
    return json_value(std::move(state));
}

std::size_t json_value::size() const {
    const auto& node = *require_state().node;
    if (node.is_array()) {
        return node.as_array().size();
    }
    if (node.is_object()) {
        return node.as_object().size();
    }
    throw std::runtime_error("json_value::size requires array or object");
}

std::string json_value::dump() const {
    return boost::json::serialize(*require_state().node);
}

json_value json_value::parse(std::string_view text) {
    auto parsed = boost::json::parse(text);
    auto root = std::make_shared<boost::json::value>(std::move(parsed));
    auto state = std::make_shared<json_value::state>();
    state->root = std::move(root);
    state->node = state->root.get();
    return json_value(std::move(state));
}

} // namespace warp::net::http
