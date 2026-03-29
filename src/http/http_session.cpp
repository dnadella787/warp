#include "http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/beast/http.hpp>

#include "../common/util/fail.h"
#include "warp/http/server.hpp"

namespace beast = boost::beast;   // from <boost/beast.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace warp::http {

// The socket executor is already a strand from the listener::do_accept method
http_session::http_session(boost::asio::ip::tcp::socket &&socket, registry &routes)
    : stream_(std::move(socket)), routes_(routes) {
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

void http_session::on_read(beast::error_code ec, std::size_t) {
	// This means they closed the connection
	if (ec == beast::http::error::end_of_stream)
		return shutdown();

	if (ec)
		return util::fail(ec, component, "on_read");

	warp::request request {parser_->release()};
	if (const auto handler = routes_.find(request.method(), request.target())) {
		boost::asio::co_spawn(stream_.get_executor(), (*handler)(request),
		                      beast::bind_front_handler(&http_session::on_handler_complete, shared_from_this(),
		                                                request.version(), request.keep_alive()));
	} else {
		warp::response response = response::not_found();
		response.version(request.version());
		response.keep_alive(request.keep_alive());
		response.prepare_payload();
		queue_write(std::move(response));
	}
}

void http_session::queue_write(warp::response response) {
	// Allocate and store the work
	response_queue_.push(std::move(response));

	// If there was no previous work, start the write loop
	if (response_queue_.size() == 1)
		do_write();
}

void http_session::on_handler_complete(unsigned version, bool keep_alive, std::exception_ptr eptr,
                                       warp::response response) {
	if (eptr) {
		try {
			std::rethrow_exception(eptr);
		} catch (const std::exception &ex) {
			response = warp::response::server_error(ex.what());
		} catch (...) {
			response = warp::response::server_error("Unhandled exception");
		}
	}

	response.version(version);
	response.keep_alive(keep_alive);
	response.prepare_payload();
	queue_write(std::move(response));
}

// Called to start/continue the write-loop. Should not be called when
// write_loop is already active.
void http_session::do_write() {
	if (!response_queue_.empty()) {
		bool keep_alive = response_queue_.front().keep_alive();
		beast::http::async_write(stream_, response_queue_.front(),
		                         beast::bind_front_handler(&http_session::on_write, shared_from_this(), keep_alive));
	}
}

void http_session::on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);

	if (ec)
		return util::fail(ec, component, "on_write");

	response_queue_.pop();

	if (!keep_alive)
		return shutdown();

	if (!response_queue_.empty()) {
		do_write();
		return;
	}

	do_read();
}

void http_session::shutdown() {
	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	if (ec)
		util::fail(ec, component, "shutdown");
}

} // namespace warp::http
