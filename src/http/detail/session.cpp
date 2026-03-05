#include "session.hpp"

#include <string_view>
#include <unordered_map>

#include <boost/beast/http.hpp>

#include "warp/net/http/request.hpp"

namespace warp::http::detail {

namespace {

using beast_request = boost::beast::http::request<boost::beast::http::string_body>;
using beast_response = boost::beast::http::response<boost::beast::http::string_body>;

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

std::string decode_component(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '%') {
            if (i + 2 < input.size()) {
                int hi = hex_value(input[i + 1]);
                int lo = hex_value(input[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    output.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            output.push_back(c);
        } else if (c == '+') {
            output.push_back(' ');
        } else {
            output.push_back(c);
        }
    }
    return output;
}

std::unordered_map<std::string, std::string> parse_query(std::string_view query) {
    std::unordered_map<std::string, std::string> params;
    std::size_t start = 0;
    while (start < query.size()) {
        auto end = query.find('&', start);
        auto token = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!token.empty()) {
            auto eq = token.find('=');
            auto key_view = token.substr(0, eq);
            auto value_view = eq == std::string::npos ? std::string_view{} : token.substr(eq + 1);
            auto key = decode_component(key_view);
            auto value = decode_component(value_view);
            if (!key.empty()) {
                params[std::move(key)] = std::move(value);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return params;
}

net::http::method map_method(boost::beast::http::verb v) {
    using verb = boost::beast::http::verb;
    switch (v) {
    case verb::get: return net::http::method::get;
    case verb::post: return net::http::method::post;
    case verb::put: return net::http::method::put;
    case verb::delete_: return net::http::method::delete_;
    case verb::head: return net::http::method::head;
    case verb::options: return net::http::method::options;
    case verb::patch: return net::http::method::patch;
    default: return net::http::method::unknown;
    }
}

net::http::request to_request(const beast_request& req) {
    net::http::headers hdrs;
    for (const auto& field : req.base()) {
        hdrs.emplace(field.name_string(), field.value());
    }
    auto target = std::string(req.target());
    std::string path = target;
    std::unordered_map<std::string, std::string> query_params;
    if (auto pos = target.find('?'); pos != std::string::npos) {
        path = target.substr(0, pos);
        auto query_view = std::string_view(target).substr(pos + 1);
        query_params = parse_query(query_view);
    }
    net::http::request warp_req(map_method(req.method()), std::move(target), req.body(), std::move(hdrs));
    warp_req.set_path(std::move(path));
    warp_req.set_query_params(std::move(query_params));
    return warp_req;
}

std::shared_ptr<beast_response> to_beast_response(const net::http::response& resp, const beast_request& req) {
    auto be_resp = std::make_shared<beast_response>();
    be_resp->version(req.version());
    be_resp->result(resp.status());
    be_resp->body() = std::string(resp.body());
    for (const auto& [key, value] : resp.header_map()) {
        auto field = boost::beast::http::string_to_field(key);
        if (field == boost::beast::http::field::unknown) {
            be_resp->set(key, value);
        } else {
            be_resp->set(field, value);
        }
    }
    be_resp->prepare_payload();
    if (!be_resp->has_content_length()) {
        be_resp->content_length(be_resp->body().size());
    }
    be_resp->keep_alive(false);
    return be_resp;
}

} // namespace

session::session(boost::asio::ip::tcp::socket socket, net::router::registry& routes)
    : stream_(std::move(socket))
    , routes_(routes) {}

void session::start() {
    read();
}

void session::read() {
    request_ = {};
    boost::beast::http::async_read(stream_, buffer_, request_,
        [self = shared_from_this()](boost::beast::error_code ec, std::size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        });
}

void session::on_read(boost::beast::error_code ec, std::size_t) {
    if (ec == boost::beast::http::error::end_of_stream) {
        shutdown();
        return;
    }
    if (ec) {
        return;
    }

    auto warp_request = to_request(request_);
    net::http::response resp;
    if (auto match = routes_.find(warp_request.path())) {
        warp_request.set_path_params(std::move(match->params));
        resp = match->handler(warp_request);
    } else {
        resp = net::http::response::not_found();
    }

    write_response(std::move(resp));
}

void session::write_response(net::http::response resp) {
    auto be_resp = to_beast_response(resp, request_);
    const bool close = !be_resp->keep_alive();
    boost::beast::http::async_write(stream_, *be_resp,
        [self = shared_from_this(), be_resp, close](boost::beast::error_code ec, std::size_t bytes_transferred) {
            self->on_write(ec, bytes_transferred, close);
        });
}

void session::on_write(boost::beast::error_code ec, std::size_t, bool close) {
    if (ec) {
        return;
    }
    if (close) {
        shutdown();
        return;
    }
    read();
}

void session::shutdown() {
    boost::system::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}

} // namespace warp::http::detail
