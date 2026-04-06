#include "http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/beast/http.hpp>

#include "../../common/util/fail.h"
#include "../../common/util/lambda.h"

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
	                      beast::bind_front_handler(&http_session::maybe_read, this->shared_from_this()));
}

void http_session::maybe_read() {
	// a write/read error or Connection: close path calls shutdown() and shuts down the socket
	if (shutdown_started_ || stop_reading_ || read_in_progress_ || outstanding_requests_ >= pipeline_limit_) {
		return;
	}

	do_read();
}

void http_session::do_read() {
	// Construct a new parser for each message
	parser_.emplace();

	// Apply a reasonable limit to the allowed size
	// of the body in bytes to prevent abuse.
	parser_->body_limit(10000);

	// Set the timeout.
	stream_.expires_after(std::chrono::seconds(30));
	read_in_progress_ = true;

	// Read a request using the parser-oriented interface
	beast::http::async_read(stream_, buffer_, *parser_,
	                        beast::bind_front_handler(&http_session::on_read, shared_from_this()));
}

void http_session::on_read(beast::error_code ec, std::size_t) {
	read_in_progress_ = false;

	// This means they closed the connection
	if (ec == beast::http::error::end_of_stream) {
		stop_reading_ = true;
		if (outstanding_requests_ == 0 && !write_in_progress_)
			shutdown();
		return;
	}

	if (ec) {
		util::fail(ec, COMPONENT, "on_read");
		return shutdown();
	}

	warp::request request {parser_->release()};
	const auto sequence = next_request_sequence_++;
	const auto version = request.version();
	const auto keep_alive = request.keep_alive();
	++outstanding_requests_;
	if (!keep_alive)
		stop_reading_ = true;

	if (const auto *handler = routes_.find(request)) {
		std::visit(common::overloaded {
		               [&](const sync_handler &h) {
			               try {
				               auto resp = h(std::move(request));
				               on_handler_complete(sequence, version, keep_alive, nullptr, std::move(resp));
			               } catch (const std::exception &e) {
				               on_handler_complete(sequence, version, keep_alive, std::current_exception(), {});
			               }
		               },
		               [&](const async_handler &h) {
			               boost::asio::co_spawn(stream_.get_executor(), h(std::move(request)),
			                                     beast::bind_front_handler(&http_session::on_handler_complete,
			                                                               shared_from_this(), sequence, version,
			                                                               keep_alive));
		               }},
		           *handler);
	} else {
		on_handler_complete(sequence, version, keep_alive, nullptr, response::not_found());
	}

	maybe_read();
}

void http_session::maybe_write() {
	if (shutdown_started_ || write_in_progress_) {
		return;
	}

	do_write();
}

void http_session::on_handler_complete(std::size_t sequence, unsigned version, bool keep_alive, std::exception_ptr eptr,
                                       warp::response response) {
	if (shutdown_started_) {
		return;
	}

	// Unhandled exception is returned to end user as 500
	if (eptr) {
		response = warp::response::server_error();
	}

	response.version(version);
	response.keep_alive(keep_alive);
	response.prepare_payload();
	pending_responses_.emplace(sequence, std::move(response));
	maybe_write();
	maybe_read();
}

// Called to start/continue the write-loop. Should not be called when
// write_loop is already active.
void http_session::do_write() {
	const auto it = pending_responses_.find(next_write_sequence_);
	if (it != pending_responses_.end()) {
		write_in_progress_ = true;
		beast::http::async_write(stream_, it->second,
		                         beast::bind_front_handler(&http_session::on_write, shared_from_this(),
		                                                   next_write_sequence_, it->second.keep_alive()));
	}
}

void http_session::on_write(std::size_t sequence, bool keep_alive, beast::error_code ec,
                            std::size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);
	write_in_progress_ = false;

	if (ec) {
		util::fail(ec, COMPONENT, "on_write");
		return shutdown();
	}

	pending_responses_.erase(sequence);
	if (outstanding_requests_ > 0)
		--outstanding_requests_;
	++next_write_sequence_;

	if (!keep_alive) {
		stop_reading_ = true;
		return shutdown();
	}

	if (outstanding_requests_ == 0 && stop_reading_) {
		return shutdown();
	}

	maybe_write();
	maybe_read();
}

void http_session::shutdown() {
	if (shutdown_started_) {
		return;
	}
	shutdown_started_ = true;

	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	if (ec)
		util::fail(ec, COMPONENT, "shutdown");
}

} // namespace warp::http
