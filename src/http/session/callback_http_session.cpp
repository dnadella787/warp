#include "callback_http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/beast/http.hpp>

#include "../../common/util/fail.h"
#include "../../common/util/lambda.h"

namespace beast = boost::beast;   // from <boost/beast.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace warp::http {

// The socket executor is already a strand from the listener::do_accept method
callback_http_session::callback_http_session(boost::asio::ip::tcp::socket &&socket, registry &routes)
    : stream_(std::move(socket)), routes_(routes) {
}

void callback_http_session::start() {
	// We need to be executing within a strand to perform async operations
	// on the I/O objects in this session
	boost::asio::dispatch(stream_.get_executor(),
	                      beast::bind_front_handler(&callback_http_session::maybe_read, this->shared_from_this()));
}

void callback_http_session::maybe_read() {
	// 1. shutting down, stop the read loop
	// 2. stop reading (if connection: close for ex)
	// 3. read_in_progress another read already executing which will queue another, don't need to queue another here
	// 4. pipeline limit exceeded, wait till write drains a few out. Write also dequeues reads after finishing each time
	if (shutdown_started_ || stop_reading_ || read_in_progress_ || outstanding_requests_ >= pipeline_limit_) {
		return;
	}

	// above checks are to prevent reading from a shutdown socket, two in flight async_reads at the same time, or
	// too much request backpressure. If there is another async_read already in flight then we can rely on its
	// on_read completion handler to restart the loop
	do_read();
}

void callback_http_session::do_read() {
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
	                        beast::bind_front_handler(&callback_http_session::on_read, shared_from_this()));
}

void callback_http_session::on_read(beast::error_code ec, std::size_t) {
	read_in_progress_ = false;
	// client isn't sending data but we can write back
	if (ec == beast::http::error::end_of_stream) {
		stop_reading_ = true;
		// already done writing so gracefully shutdown
		if (outstanding_requests_ == 0 && !write_in_progress_)
			shutdown();
		// exit the read loop, if done writing then this ends the session
		return;
	}

	if (ec) {
		util::fail(ec, COMPONENT, "on_read");
		return shutdown(true);
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
			                                     beast::bind_front_handler(&callback_http_session::on_handler_complete,
			                                                               shared_from_this(), sequence, version,
			                                                               keep_alive));
		               }},
		           *handler);
	} else {
		on_handler_complete(sequence, version, keep_alive, nullptr, response::not_found());
	}

	maybe_read();
}

void callback_http_session::on_handler_complete(std::size_t sequence, unsigned version, bool keep_alive,
                                                std::exception_ptr eptr, warp::response response) {
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
	maybe_write(); // starts the initial write loop on the first handler completion
}

void callback_http_session::maybe_write() {
	// if the writes are stopped (either due to error or bc of close semantic + all writes finished)
	// then we exit the write loop.
	if (shutdown_started_ || write_in_progress_) {
		return;
	}

	do_write();
}

// Called to start/continue the write-loop. Should not be called when
// write_loop is already active.
void callback_http_session::do_write() {
	const auto it = pending_responses_.find(next_write_sequence_);
	if (it != pending_responses_.end()) {
		write_in_progress_ = true;
		stream_.expires_after(std::chrono::seconds(30));
		beast::http::async_write(
		    stream_, it->second,
		    beast::bind_front_handler(&callback_http_session::on_write, shared_from_this(), next_write_sequence_));
	}
}

void callback_http_session::on_write(std::size_t sequence, beast::error_code ec, std::size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);
	write_in_progress_ = false;

	if (ec) {
		util::fail(ec, COMPONENT, "on_write");
		// we error out b/c HTTP 1.1 requires resp to come in same order, so if this
		// write fails, we either have to retry this or cancel the rest too
		return shutdown(true);
	}

	pending_responses_.erase(sequence);
	++next_write_sequence_;
	--outstanding_requests_;

	// We just freed up capacity so start the read loop up again.
	if (outstanding_requests_ == pipeline_limit_ - 1)
		maybe_read();

	// no more requests to write out and not reading anymore either, just shutdown
	if (outstanding_requests_ == 0 && stop_reading_)
		return shutdown();

	maybe_write();
}

void callback_http_session::shutdown(bool force) {
	if (shutdown_started_) {
		return;
	}
	shutdown_started_ = true;

	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	if (ec)
		util::fail(ec, COMPONENT, "shutdown");

	if (force) {
		ec.clear();
		stream_.socket().close(ec);
		if (ec)
			util::fail(ec, COMPONENT, "shutdown");
	}
}

} // namespace warp::http
