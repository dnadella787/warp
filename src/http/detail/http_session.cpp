#include "http_session.hpp"

#include <string_view>
#include <unordered_map>

#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include "ws_session.h"
#include "../../util/fail.h"
#include "warp/net/http/request.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace warp::http::detail {

using beast_request = beast::http::request<beast::http::string_body>;
using beast_response = beast::http::response<beast::http::string_body>;

int hex_value(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
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
			auto value_view = eq == std::string::npos ? std::string_view {} : token.substr(eq + 1);
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

net::http::method map_method(beast::http::verb v) {
	using verb = beast::http::verb;
	switch (v) {
	case verb::get:
		return net::http::method::get;
	case verb::post:
		return net::http::method::post;
	case verb::put:
		return net::http::method::put;
	case verb::delete_:
		return net::http::method::delete_;
	case verb::head:
		return net::http::method::head;
	case verb::options:
		return net::http::method::options;
	case verb::patch:
		return net::http::method::patch;
	default:
		return net::http::method::unknown;
	}
}

net::http::request to_request(const beast_request &req) {
	net::http::headers hdrs;
	for (const auto &field : req.base()) {
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
	warp_req.set_keep_alive(req.keep_alive());
	return warp_req;
}

std::shared_ptr<beast_response> to_beast_response(const net::http::response &resp) {
	auto be_resp = std::make_shared<beast_response>();
	be_resp->version(resp.version());
	be_resp->result(resp.status());
	be_resp->body() = std::string(resp.body());
	for (const auto &[key, value] : resp.header_map()) {
		auto field = beast::http::string_to_field(key);
		if (field == beast::http::field::unknown) {
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

// The socket executor is already a strand from the listener::do_accept method
http_session::http_session(boost::asio::ip::tcp::socket &&socket, net::router::registry &routes)
    : stream_(std::move(socket)), routes_(routes) {
	static_assert(queue_limit > 0, "http session response queue limit must be > 0");
}

void http_session::start() {
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session
	boost::asio::dispatch(stream_.get_executor(),
	                      beast::bind_front_handler(&http_session::do_read, this->shared_from_this()));
}

void http_session::do_read() {
	// Construct a new parser for each message
	parser_.emplace();

	// Apply a reasonable limit to the allowed size
	// of the body in bytes to prevent abuse.
	parser_->body_limit(10000);

	// Set the timeout.
	stream_.expires_after(std::chrono::seconds(30));

	// Read a request using the parser-oriented interface
	beast::http::async_read(stream_, buffer_, *parser_,
	                        beast::bind_front_handler(&http_session::on_read, shared_from_this()));
}

void http_session::on_read(beast::error_code &ec, std::size_t) {
	// This means they closed the connection
	if (ec == beast::http::error::end_of_stream)
		return shutdown();

	if (ec)
		return util::fail(ec, component, "on_read");

	// See if it is a WebSocket Upgrade
	if (websocket::is_upgrade(parser_->get())) {
		// Create a websocket session, transferring ownership
		// of both the socket and the HTTP request.
		std::make_shared<ws_session>(stream_.release_socket())->do_accept(parser_->release());
		return;
	}

	auto warp_request = to_request(parser_->release());
	net::http::response resp;
	if (const auto match = routes_.find(warp_request.path())) {
		resp = match->handler(warp_request);
		resp.set_keep_alive(warp_request.keep_alive());
	} else {
		resp = net::http::response::not_found();
	}

	queue_write(std::move(resp));

	// If we aren't at the queue limit, try to pipeline another request
	if (response_queue_.size() < queue_limit)
		do_read();
}

void http_session::queue_write(net::http::response response) {
	// Allocate and store the work
	response_queue_.push(std::move(response));

	// If there was no previous work, start the write loop
	if (response_queue_.size() == 1)
		do_write();
}

void http_session::write_response(const net::http::response &resp) {
	auto be_resp = to_beast_response(resp);
	const bool close = !be_resp->keep_alive();
	beast::http::async_write(
	    stream_, *be_resp,
	    [self = shared_from_this(), be_resp, close](beast::error_code ec, std::size_t bytes_transferred) {
		    self->on_write(bytes_transferred, close, ec);
	    });
}

// Called to start/continue the write-loop. Should not be called when
// write_loop is already active.
void http_session::do_write() {
	if(!response_queue_.empty())
	{
		bool keep_alive = response_queue_.front().keep_alive();
		auto beast_resp = to_beast_response( response_queue_.front());

		beast::async_write(
			stream_,
			std::move(response_queue_.front()),
			beast::bind_front_handler(
				&http_session::on_write,
				shared_from_this(),
				keep_alive));
	}
}

void http_session::on_write(std::size_t bytes_transferred, bool keep_alive, beast::error_code ec) {
		boost::ignore_unused(bytes_transferred);

		if(ec)
			return util::fail(ec, component, "on_write");

		if(!keep_alive)
			return shutdown();

		// Resume the read if it has been paused
		if(response_queue_.size() == queue_limit)
			do_read();

		response_queue_.pop();

		do_write();
	}


void http_session::shutdown() {
	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	util::fail(ec, component, "shutdown");
}

} // namespace warp::http::detail
