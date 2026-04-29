#include "callback_http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/beast/http.hpp>

#include "warp/logging/logger.hpp"
#include "../../common/util/lambda.h"

namespace beast = boost::beast;   // from <boost/beast.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace warp::server {

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
	if (shutdown_started_ || stop_reading_ || read_in_progress_ || outstanding_requests_ >= pipeline_limit_)
		return;

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
		log::error("Error in {} during {}: {}", COMPONENT, "on_read", ec.message());
		return shutdown(true);
	}

	// async_read was canceled after it was started so just exit the read loop instead of letting the now invalid
	// request have its mapped handler execute and potentially mutate data
	// (resp 1 closes connection but async_read 2 already kicked off before req 1 handler returned resp 1)
	// TODO: maybe cancel the outstanding async_read with a forceful shutdown so we can more aggressively shutdown TCP
	// connections to accept more requests
	if (stop_reading_ || shutdown_started_)
		return parser_.reset();

	request request {parser_->release()};
	const std::size_t sequence = next_request_sequence_++;
	request_ctxs_.emplace(sequence, request_context {.sequence = sequence,
	                                                 .version = request.version(),
	                                                 .client_keep_alive = request.keep_alive()});
	++outstanding_requests_;

	close_policy_.on_request_accepted(sequence, request.keep_alive());
	if (!close_policy_.accepting_requests())
		stop_reading_ = true;

	if (const auto *handler = routes_.find(request)) {
		std::visit(common::overloaded {[&](const http::sync_handler &h) {
			                               try {
				                               auto resp = h(std::move(request));
				                               on_handler_complete(sequence, nullptr, std::move(resp));
			                               } catch (const std::exception &e) {
				                               on_handler_complete(sequence, std::current_exception(), {});
			                               }
		                               },
		                               [&](const http::async_handler &h) {
			                               boost::asio::co_spawn(
			                                   stream_.get_executor(), h(std::move(request)),
			                                   beast::bind_front_handler(&callback_http_session::on_handler_complete,
			                                                             shared_from_this(), sequence));
		                               }},
		           *handler);
	} else {
		on_handler_complete(sequence, nullptr, http::response::not_found());
	}

	maybe_read();
}

void callback_http_session::on_handler_complete(std::size_t sequence, std::exception_ptr eptr,
                                                warp::response response) {
	// shutdown could be initiated during the async request handler execution in
	// which case we should dump this response because we already told the client we are not
	// writing out anymore responses
	if (shutdown_started_)
		return;

	auto ctx_it = request_ctxs_.find(sequence);
	if (ctx_it == request_ctxs_.end())
		return log::error("Error in {} during {}: {}", COMPONENT, "on_handler_complete{req_ctx.find}",
		                  "context could not be found in session map on completion");

	// Unhandled exception is returned to end user as 500
	// TODO: set keep alive to false for uncaught exceptions but let user configure that
	if (eptr)
		response = http::response::server_error();

	const auto decision = close_policy_.on_response_ready(ctx_it->second, response);
	if (!close_policy_.accepting_requests())
		stop_reading_ = true;

	// drop the response because client or server initiated connection close prior to this request
	if (decision.drop_response) {
		// erase it from request ctx pool, if we are done just shutdown
		finish_request(ctx_it->second.sequence);
		if (outstanding_requests_ == 0 and stop_reading_)
			return shutdown();

		// otherwise keep writing out any remaining responses
		maybe_write();
		return;
	}

	response.prepare_payload();
	pending_responses_.emplace(
	    sequence, pending_write {.response = std::move(response), .close_after_write = decision.close_after_write});
	maybe_write(); // starts the initial write loop on the first handler completion
}

void callback_http_session::maybe_write() {
	// if the writes are stopped (either due to error or bc of close semantic + all writes finished)
	// then we exit the write loop.
	if (shutdown_started_ || write_in_progress_)
		return;

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
		    stream_, it->second.response,
		    beast::bind_front_handler(&callback_http_session::on_write, shared_from_this(), next_write_sequence_));
	}
	// else the next write sequence is not available to write out so we stop the write loop.
	// the completion handler will start the write loop back up later
}

void callback_http_session::on_write(std::size_t sequence, beast::error_code ec, std::size_t _) {
	write_in_progress_ = false;

	if (ec) {
		log::error("Error in {} during {}: {}", COMPONENT, "on_write", ec.message());
		// we error out b/c HTTP 1.1 requires resp to come in same order, so if this
		// write fails, we either have to retry this or cancel the rest too
		return shutdown(true);
	}

	auto it = pending_responses_.find(sequence);
	bool close_after_write {true};
	if (it != pending_responses_.end()) {
		close_after_write = it->second.close_after_write;
		pending_responses_.erase(sequence);
	} else {
		log::error("Error in {} during {}: {}", COMPONENT, "on_write{pending_responses.find}",
		           "could not find response in pending response map to erase");
	}

	// increment write sequence so we look to write the next response out next time
	++next_write_sequence_;
	finish_request(sequence);

	// close after this write (b/c client/server said connection close)
	// we already finished the write so shutdown
	if (close_after_write)
		return shutdown();

	// We just freed up capacity so start the read loop up again.
	if (outstanding_requests_ == pipeline_limit_ - 1)
		maybe_read();

	// not reading in anymore and nothing else to process/write out so just shutdown
	if (outstanding_requests_ == 0 && stop_reading_)
		return shutdown();

	// keep the loop going
	maybe_write();
}

void callback_http_session::finish_request(std::size_t sequence) {
	request_ctxs_.erase(sequence);
	--outstanding_requests_;
}

void callback_http_session::shutdown(bool force) {
	if (shutdown_started_) {
		return;
	}
	shutdown_started_ = true;

	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	if (ec)
		log::error("Error in {} during {}: {}", COMPONENT, "shutdown", ec.message());

	if (force) {
		ec.clear();
		stream_.socket().close(ec);
		if (ec)
			log::error("Error in {} during {}: {}", COMPONENT, "shutdown", ec.message());
	}
}

} // namespace warp::server
