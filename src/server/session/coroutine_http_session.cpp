#include "coroutine_http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>

#include "callback_http_session.hpp"
#include "common/util/lambda.h"

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

namespace warp::server {

coroutine_http_session::coroutine_http_session(boost::asio::ip::tcp::socket &&socket, const registry &routes,
                                               const interceptor_chain &interceptor_chain, log::logger logger)
    : stream_(std::move(socket)), routes_(routes), interceptor_chain_(interceptor_chain), logger_(std::move(logger)),
      read_signal_(stream_.get_executor()), write_signal_(stream_.get_executor()) {
}

void coroutine_http_session::start() {
	// start the read loop on another coroutine
	boost::asio::co_spawn(
	    stream_.get_executor(),
	    [self = shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->read_loop(); },
	    boost::asio::detached);

	// start the write loop on a coroutine, note that this immediately suspends and waits till
	// the first response is ready to go be written out
	boost::asio::co_spawn(
	    stream_.get_executor(),
	    [self = shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->write_loop(); },
	    boost::asio::detached);
}

boost::asio::awaitable<void> coroutine_http_session::read_loop() {
	for (;;) {
		// if we are still allowed to read but cannot because the pipeline is full, suspend
		// 1. socket is NOT shutdown and
		// 2. reading continues (no client or server side connection close established yet) and
		// 3. write pipeline is full
		while (!shutdown_started_ && !stop_reading_ && outstanding_requests_ >= pipeline_limit_)
			co_await wait_for_read_ready();

		// read loop either unsuspended, reading was stopped, or session is ending
		if (shutdown_started_ || stop_reading_) {
			// not reading anymore and no more requests to read out, just shutdown completely and exit
			if (stop_reading_ && outstanding_requests_ == 0)
				shutdown();
			co_return;
		}

		parser_.emplace();
		parser_->body_limit(10000);
		stream_.expires_after(std::chrono::seconds(30));

		beast::error_code ec;
		co_await beast::http::async_read(stream_, buffer_, *parser_,
		                                 boost::asio::redirect_error(boost::asio::use_awaitable, ec));

		// last request sent by client and now they are ending it
		if (ec == beast::http::error::end_of_stream) {
			stop_reading_ = true;
			// if there are no more requests to write out, then we can shutdown gracefully before returning
			// otherwise the read loop ends but the write loop continues to write out pending responses
			if (outstanding_requests_ == 0)
				shutdown();
			co_return;
		}

		if (ec) {
			logger_.error("Error in coroutine_http_session during read_loop: {}", ec.message());
			co_return shutdown();
		}

		// async_read was canceled after it was started so just exit the read loop instead of letting the now invalid
		// request have its mapped handler execute and potentially mutate data
		// (resp 1 closes connection but async_read 2 already kicked off before req 1 handler returned resp 1)
		// TODO: maybe cancel the outstanding async_read with a forceful shutdown so we can more aggressively shutdown
		// TCP connections to accept more requests
		if (shutdown_started_ || stop_reading_)
			co_return parser_.reset();

		http::request req {parser_->release()};
		const auto sequence = next_request_sequence_++;
		request_ctxs_.emplace(
		    sequence,
		    request_context {.sequence = sequence, .version = req.version(), .client_keep_alive = req.keep_alive()});
		++outstanding_requests_;

		// checks if client wants to keep the connection alive or close it
		policy_.on_request_accepted(sequence, req.keep_alive());
		if (!policy_.accepting_requests())
			stop_reading_ = true;

		if (const auto *handler = routes_.find(req)) {
			std::visit(common::overloaded {
			               [&](const http::sync_handler &h) { execute_sync_handler(sequence, h, std::move(req)); },
			               [&](const http::async_handler &h) {
				               boost::asio::co_spawn(
				                   stream_.get_executor(), // little trick to extend http_session lifetime by
				                                           // passing it to async coroutine
				                   execute_async_handler(shared_from_this(), sequence, h, std::move(req)),
				                   boost::asio::detached);
			               }},
			           *handler);
		} else {
			complete_request(sequence, http::response::not_found());
		}
	}
}

boost::asio::awaitable<void> coroutine_http_session::write_loop() {
	for (;;) {
		std::map<std::size_t, pending_write>::iterator it;
		// as long as the server side session has not initiated shutdown (no error/pending requests)
		while (!shutdown_started_) {
			it = pending_responses_.find(next_write_sequence_);
			// we check if the next required response (b/c HTTP 1.1 requires responses in the same order)
			// is ready,
			if (it != pending_responses_.end())
				break;
			// if its not ready, we check if we even have to write anything else or not, if not, we shutdown completely
			// and exit the write loop
			if (stop_reading_ && outstanding_requests_ == 0)
				co_return shutdown();
			// otherwise there are more pending writes but not ready yet, wait for them to become ready
			co_await wait_for_write_ready();
		}

		// if the server side session has initiated shutdown, then we just exit, no more writing to do
		if (shutdown_started_)
			co_return;

		stream_.expires_after(std::chrono::seconds(30));
		beast::error_code ec;
		co_await beast::http::async_write(stream_, it->second.response,
		                                  boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			logger_.error("Error in coroutine_http_session during write_loop: {}", ec.message());
			co_return shutdown();
		}

		// we made the decision earlier when the handler completed on whether to close the socket after this write or
		// not (based on client keep-alive && server keep-alive)
		const bool close_after_write = it->second.close_after_write;
		// response written out we can wipe it from the write ledger
		pending_responses_.erase(it);
		// decrement outstanding_request count and erase it from the req ledger as well (used to compute closing
		// policies)
		finish_request(next_write_sequence_++);
		// if we decided this socket needs to be closed (server wants to close the connection or client has decided that
		// this is the last request) then shutdown the session (we already wrote this out)
		if (close_after_write)
			co_return shutdown();

		//
		if (stop_reading_ && outstanding_requests_ == 0)
			co_return shutdown();

		// restart the read loop in case it is suspended (it will only impact the read loop waiting if
		// its waiting because the pipeline limit is reached and the write loop was able to successfully
		// drain the queue to let more reads execute).
		notify_read_loop();
	}
}

// Uses a steady_timer as a manual event: set to max() so it never expires.
// Coroutine suspends on async_wait() and will not resume on its own.
// Resumes only when read_signal_.cancel() completes the wait (ec set, no throw).
// No persistence: cancel() before waiting is lost; only active waiters are woken.
boost::asio::awaitable<void> coroutine_http_session::wait_for_read_ready() {
	read_signal_.expires_at(boost::asio::steady_timer::time_point::max());
	beast::error_code ec;
	co_await read_signal_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
}

void coroutine_http_session::notify_read_loop() {
	read_signal_.cancel();
}

// Same pattern for write readiness using a timer as a signal.
// Blocks indefinitely until write_signal_.cancel() is invoked elsewhere.
// redirect_error captures operation_aborted in ec instead of throwing.
// Wakes all current waiters; no queued signals for future waiters.
boost::asio::awaitable<void> coroutine_http_session::wait_for_write_ready() {
	write_signal_.expires_at(boost::asio::steady_timer::time_point::max());
	beast::error_code ec;
	co_await write_signal_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
}

void coroutine_http_session::notify_write_loop() {
	write_signal_.cancel();
}

void coroutine_http_session::execute_sync_handler(std::size_t sequence, const http::sync_handler &handler,
                                                  http::request req) {
	try {
		auto resp = handler(std::move(req));
		complete_request(sequence, std::move(resp));
	} catch (...) {
		complete_request(sequence, http::response::server_error());
	}
}

boost::asio::awaitable<void> coroutine_http_session::execute_async_handler(std::shared_ptr<coroutine_http_session> self,
                                                                           std::size_t sequence,
                                                                           const http::async_handler &handler,
                                                                           http::request req) {
	try {
		auto resp = co_await handler(std::move(req));
		self->complete_request(sequence, std::move(resp));
	} catch (...) {
		self->complete_request(sequence, http::response::server_error());
	}
}

void coroutine_http_session::complete_request(std::size_t sequence, http::response response) {
	if (shutdown_started_)
		return;

	auto ctx_it = request_ctxs_.find(sequence);
	if (ctx_it == request_ctxs_.end())
		return logger_.error("Error in coroutine_http_session during on_handler_complete{{req_ctx.find}}: {}",
		                     "context could not be found in session map on completion");

	// we checked before executing the handler to see if client wants to keep connection alive or close it,
	// now we check the server's own decision before setting the decision based on both (client && server)
	const auto [drop_response, close_after_write] = policy_.on_response_ready(ctx_it->second, response);
	if (!policy_.accepting_requests())
		stop_reading_ = true;

	// of course there is the chance that a request from the server that finished late wanted to close the conenction,
	// in that case a request that finished before but arrived later will need to be dropped as well
	if (drop_response) {
		finish_request(sequence);
		// if no more requests to process + write out, we can just shutdown
		if (outstanding_requests_ == 0 && stop_reading_)
			shutdown();
		return;
	}

	response.prepare_payload();
	pending_responses_.emplace(sequence,
	                           pending_write {.response = std::move(response), .close_after_write = close_after_write});

	// start the write loop because it will immediately suspend while waiting on the proper response (i.e. first one) to
	// be available. If a later request finishes first then the loop just checks and goes back to sleep awaiting the
	// proper one.
	//
	// on subsequent runs it pretty much does nothing (the write_signal_.cancel() gets lost because the write loop is
	// not actually awaiting anything)
	notify_write_loop();
}

void coroutine_http_session::shutdown() {
	// shutdown already initiated
	if (shutdown_started_)
		return;
	shutdown_started_ = true;
	// if the read loop is waiting on write pipeline to be drained or there is already an async_read outstanding
	// then we cancel it
	read_signal_.cancel();
	write_signal_.cancel();

	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	if (ec && ec != boost::asio::error::not_connected)
		logger_.error("Error in coroutine_http_session during shutdown: {}", ec.message());
}

void coroutine_http_session::finish_request(std::size_t sequence) {
	request_ctxs_.erase(sequence);
	--outstanding_requests_;
}

} // namespace warp::server
