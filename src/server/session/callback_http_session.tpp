#include <boost/asio/co_spawn.hpp>
#include <boost/beast/http.hpp>

#include "callback_http_session.hpp"

namespace beast = boost::beast;   // from <boost/beast.hpp>

namespace warp::server {

// The socket executor is already a strand from the listener::do_accept method
template <warp_session_transport Transport>
callback_http_session<Transport>::callback_http_session(boost::asio::ip::tcp::socket &&socket, Transport transport,
                                                        const route_runtime_t &routes,
                                                        const interceptor_chain<request> &req_chain,
                                                        const interceptor_chain<response> &resp_chain,
                                                        log::logger logger)
    : transport_(std::move(transport)),
      stream_(transport_.make_stream(std::move(socket))), routes_(routes), req_interceptor_chain_(req_chain),
      resp_interceptor_chain_(resp_chain), logger_(std::move(logger)) {
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::start() {
	Transport::start(*this);
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::fail_transport_start(std::string_view stage, beast::error_code ec) {
	logger_.error("error in callback_http_session during {}: {}", stage, ec.message());
	abort_transport();
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::maybe_read() {
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

template <warp_session_transport Transport>
void callback_http_session<Transport>::do_read() {
	// Construct a new parser for each message
	parser_.emplace();

	// Apply a reasonable limit to the allowed size
	// of the body in bytes to prevent abuse.
	parser_->body_limit(10000);

	// Set the timeout (asio::ssl::stream does not have expires_after, so we search for the underlying socket)
	beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
	read_in_progress_ = true;

	// Read a request using the parser-oriented interface
	beast::http::async_read(stream_, buffer_, *parser_,
	                        beast::bind_front_handler(&callback_http_session::on_read,
	                                                  this->shared_from_this()));
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::on_read(beast::error_code ec, std::size_t) {
	read_in_progress_ = false;
	// client isn't sending data but we can write back
	// note that for 
	if (ec == beast::http::error::end_of_stream || Transport::should_treat_read_error_as_eof(ec)) {
		stop_reading_ = true;
		// already done writing so gracefully shutdown
		if (outstanding_requests_ == 0 && !write_in_progress_)
			graceful_shutdown();
		// exit the read loop, if done writing then this ends the session
		return;
	}

	if (ec) {
		logger_.error("error in callback_http_session during on_read: {}", ec.message());
		return abort_transport();
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

	/**
	 * TODO: Issue#36
	 * The proper way to do this is to permanently get rid of the sync handler and just always execute using
	 * async handlers (i.e. always kick off a coroutine). The order of execution on the coroutine should go:
	 *
	 * req_ctx = req_ctx { .request = request }
	 * auto match = routes_.find(request)
	 * req_ctx.match_found = match.found()
	 *
	 * universal_request_interceptor_chain_.run_interceptors(req_ctx)
	 * if (match.found()) {
	 *		matched_request_interceptor_chain_.run_interceptors(req_ctx)
	 *  	routes_.dispatch(match->id, *this, sequence, std::move(request));
	 * }
	 *
	 * and then in the completion handler:
	 * universal_resp_interceptor_chain_.run(req_ctx, response)
	 * if (match.found()) {
	 *		matched_response_interceptor_chain_.run_interceptors(req_ctx)
	 */
	if (const auto match = routes_.find(request)) {
		routes_.dispatch(match->id, *this, sequence, std::move(request));
	} else {
		on_handler_complete(sequence, nullptr, http::response::not_found());
	}

	maybe_read();
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::dispatch_sync_handler(std::size_t sequence, const http::sync_handler &handler,
                                                             http::request request) {
	try {
		response response;
		if (auto intercepted = req_interceptor_chain_.run(request); intercepted.has_value())
			response = std::move(*intercepted);
		else
			response = handler(std::move(request));

		on_handler_complete(sequence, nullptr, std::move(response));
	} catch (...) {
		return on_handler_complete(sequence, std::current_exception(), {});
	}
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::dispatch_async_handler(std::size_t sequence, const http::async_handler &handler,
                                                              http::request request) {
	boost::asio::co_spawn(
	    stream_.get_executor(), run_async_handler(this->shared_from_this(), handler, std::move(request)),
	    beast::bind_front_handler(&callback_http_session::on_handler_complete,
	                              this->shared_from_this(), sequence));
}

template <warp_session_transport Transport>
boost::asio::awaitable<http::response>
callback_http_session<Transport>::run_async_handler(std::shared_ptr<callback_http_session> self,
                                                    const http::async_handler &handler, http::request req) {
	if (auto intercepted = self->req_interceptor_chain_.run(req); intercepted.has_value())
		co_return std::move(*intercepted);

	co_return co_await handler(std::move(req));
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::on_handler_complete(std::size_t sequence, std::exception_ptr eptr,
                                                           response response) {
	// shutdown could be initiated during the async request handler execution in
	// which case we should dump this response because we already told the client we are not
	// writing out anymore responses
	if (shutdown_started_)
		return;

	auto ctx_it = request_ctxs_.find(sequence);
	if (ctx_it == request_ctxs_.end())
		return logger_.trace("error in callback_http_session during on_handler_complete{{req_ctx.find}}: {}",
		                     "context could not be found in session map on completion");

	// Unhandled exception is returned to end user as 500
	// TODO: set keep alive to false for uncaught exceptions but let user configure that
	if (eptr)
		response = http::response::server_error();

	if (!close_policy_.should_drop_response(ctx_it->second)) {
		// we run the resp interceptor chain as long as the request should not be dropped because a
		// prior request already set the close market further ahead. We decide the actual policy later on
		// since its possible the response interceptor to mutate the keep-alive response values
		//
		// If the resp chain itself throws an error, just return 5xx, don't rerun it.
		try {
			resp_interceptor_chain_.run(response);
		} catch (...) {
			response = http::response::server_error();
		}
	}

	const auto decision = close_policy_.on_response_ready(ctx_it->second, response);
	if (!close_policy_.accepting_requests())
		stop_reading_ = true;

	// drop the response because client or server initiated connection close prior to this request
	if (decision.drop_response) {
		// erase it from request ctx pool, if we are done just shutdown
		finish_request(ctx_it->second.sequence);
		if (outstanding_requests_ == 0 and stop_reading_)
			return graceful_shutdown();

		// otherwise keep writing out any remaining responses
		maybe_write();
		return;
	}

	response.prepare_payload();
	pending_responses_.emplace(
	    sequence, pending_write {.response = std::move(response), .close_after_write = decision.close_after_write});
	maybe_write(); // starts the initial write loop on the first handler completion
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::maybe_write() {
	// if the writes are stopped (either due to error or bc of close semantic + all writes finished)
	// then we exit the write loop.
	if (shutdown_started_ || write_in_progress_)
		return;

	do_write();
}

// Called to start/continue the write-loop. Should not be called when
// write_loop is already active.
template <warp_session_transport Transport>
void callback_http_session<Transport>::do_write() {
	const auto it = pending_responses_.find(next_write_sequence_);
	if (it != pending_responses_.end()) {
		write_in_progress_ = true;
		beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
		beast::http::async_write(
		    stream_, it->second.response,
		    beast::bind_front_handler(&callback_http_session::on_write, this->shared_from_this(),
		                              next_write_sequence_));
	}
	// else the next write sequence is not available to write out so we stop the write loop.
	// the completion handler will start the write loop back up later
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::on_write(std::size_t sequence, beast::error_code ec, std::size_t _) {
	write_in_progress_ = false;

	if (ec) {
		logger_.trace("error in callback_http_session during on_write: {}", ec.message());
		// we error out b/c HTTP 1.1 requires resp to come in same order, so if this
		// write fails, we either have to retry this or cancel the rest too
		return abort_transport();
	}

	auto it = pending_responses_.find(sequence);
	bool close_after_write {true};
	if (it != pending_responses_.end()) {
		close_after_write = it->second.close_after_write;
		pending_responses_.erase(sequence);
	} else {
		logger_.trace("error in callback_http_session during on_write{{pending_responses.find}}: {}",
		              "could not find response in pending response map to erase");
	}

	// increment write sequence so we look to write the next response out next time
	++next_write_sequence_;
	finish_request(sequence);

	// close after this write (b/c client/server said connection close)
	// we already finished the write so shutdown
	if (close_after_write)
		return graceful_shutdown();

	// We just freed up capacity so start the read loop up again.
	if (outstanding_requests_ == pipeline_limit_ - 1)
		maybe_read();

	// not reading in anymore and nothing else to process/write out so just shutdown
	if (outstanding_requests_ == 0 && stop_reading_)
		return graceful_shutdown();

	// keep the loop going
	maybe_write();
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::finish_request(std::size_t sequence) {
	request_ctxs_.erase(sequence);
	--outstanding_requests_;
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::graceful_shutdown() {
	if (shutdown_started_) {
		return;
	}
	shutdown_started_ = true;
	Transport::graceful_shutdown(*this);
}

template <warp_session_transport Transport>
void callback_http_session<Transport>::abort_transport() {
	if (shutdown_started_) {
		return;
	}
	shutdown_started_ = true;
	Transport::abort(stream_);
}

} // namespace warp::server
