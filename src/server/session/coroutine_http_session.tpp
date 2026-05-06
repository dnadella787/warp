#include "coroutine_http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>

namespace beast = boost::beast;

namespace warp::server {

template <warp_session_transport Transport>
coroutine_http_session<Transport>::coroutine_http_session(
    boost::asio::ip::tcp::socket &&socket, Transport transport, const route_runtime_t &routes,
    const interceptor_chain<request> &req_chain, const interceptor_chain<response> &resp_chain, log::logger logger)
    : transport_(std::move(transport)), stream_(transport_.make_stream(std::move(socket))), routes_(routes),
      req_interceptor_chain_(req_chain), resp_interceptor_chain_(resp_chain), logger_(std::move(logger)),
      read_signal_(stream_.get_executor()), write_signal_(stream_.get_executor()) {
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::start() {
	Transport::start(*this);
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::fail_transport_start(std::string_view stage, boost::beast::error_code ec) {
	logger_.error("error in coroutine_http_session during {}: {}", stage, ec.message());
	abort_transport();
}

template <warp_session_transport Transport>
boost::asio::awaitable<void> coroutine_http_session<Transport>::read_loop() {
	for (;;) {
		while (!shutdown_started_ && !stop_reading_ && outstanding_requests_ >= pipeline_limit_)
			co_await wait_for_read_ready();

		if (shutdown_started_ || stop_reading_) {
			if (stop_reading_ && outstanding_requests_ == 0)
				graceful_shutdown();
			co_return;
		}

		parser_.emplace();
		parser_->body_limit(10000);
		beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

		beast::error_code ec;
		co_await beast::http::async_read(stream_, buffer_, *parser_,
		                                 boost::asio::redirect_error(boost::asio::use_awaitable, ec));

		if (ec == beast::http::error::end_of_stream) {
			stop_reading_ = true;
			if (outstanding_requests_ == 0)
				graceful_shutdown();
			co_return;
		}
		if (Transport::should_treat_read_error_as_eof(ec)) {
			stop_reading_ = true;
			if (outstanding_requests_ == 0)
				graceful_shutdown();
			co_return;
		}

		if (ec) {
			logger_.error("error in coroutine_http_session during read_loop: {}", ec.message());
			abort_transport();
			co_return;
		}

		if (shutdown_started_ || stop_reading_) {
			parser_.reset();
			co_return;
		}

		http::request req {parser_->release()};
		const auto sequence = next_request_sequence_++;
		request_ctxs_.emplace(
		    sequence,
		    request_context {.sequence = sequence, .version = req.version(), .client_keep_alive = req.keep_alive()});
		++outstanding_requests_;

		close_policy_.on_request_accepted(sequence, req.keep_alive());
		if (!close_policy_.accepting_requests())
			stop_reading_ = true;

		if (const auto match = routes_.find(req)) {
			routes_.dispatch(match->id, *this, sequence, std::move(req));
		} else {
			complete_request(sequence, http::response::not_found());
		}
	}
}

template <warp_session_transport Transport>
boost::asio::awaitable<void> coroutine_http_session<Transport>::write_loop() {
	for (;;) {
		std::map<std::size_t, pending_write>::iterator it;
		while (!shutdown_started_) {
			it = pending_responses_.find(next_write_sequence_);
			if (it != pending_responses_.end())
				break;
			if (stop_reading_ && outstanding_requests_ == 0) {
				graceful_shutdown();
				co_return;
			}
			co_await wait_for_write_ready();
		}

		if (shutdown_started_)
			co_return;

		beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
		beast::error_code ec;
		co_await beast::http::async_write(stream_, it->second.response,
		                                  boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			logger_.error("error in coroutine_http_session during write_loop: {}", ec.message());
			abort_transport();
			co_return;
		}

		const bool close_after_write = it->second.close_after_write;
		pending_responses_.erase(it);
		finish_request(next_write_sequence_++);
		if (close_after_write) {
			graceful_shutdown();
			co_return;
		}

		if (stop_reading_ && outstanding_requests_ == 0) {
			graceful_shutdown();
			co_return;
		}

		notify_read_loop();
	}
}

template <warp_session_transport Transport>
boost::asio::awaitable<void> coroutine_http_session<Transport>::wait_for_read_ready() {
	read_signal_.expires_at(boost::asio::steady_timer::time_point::max());
	beast::error_code ec;
	co_await read_signal_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::notify_read_loop() {
	read_signal_.cancel();
}

template <warp_session_transport Transport>
boost::asio::awaitable<void> coroutine_http_session<Transport>::wait_for_write_ready() {
	write_signal_.expires_at(boost::asio::steady_timer::time_point::max());
	beast::error_code ec;
	co_await write_signal_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::notify_write_loop() {
	write_signal_.cancel();
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::dispatch_sync_handler(std::size_t sequence, const http::sync_handler &handler,
                                                              http::request req) {
	response resp;
	try {
		if (auto intercepted = req_interceptor_chain_.run(req); intercepted.has_value()) {
			resp = std::move(*intercepted);
		} else {
			resp = handler(std::move(req));
		}
	} catch (...) {
		resp = response::server_error();
	}

	complete_request(sequence, std::move(resp));
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::dispatch_async_handler(std::size_t sequence, const http::async_handler &handler,
                                                               http::request req) {
	boost::asio::co_spawn(stream_.get_executor(),
	                      run_async_handler(this->shared_from_this(), sequence, handler, std::move(req)),
	                      boost::asio::detached);
}

template <warp_session_transport Transport>
boost::asio::awaitable<void> coroutine_http_session<Transport>::run_async_handler(
    std::shared_ptr<coroutine_http_session> self, std::size_t sequence, const http::async_handler &handler,
    http::request req) {
	response resp;
	try {
		if (auto intercepted = self->req_interceptor_chain_.run(req); intercepted.has_value()) {
			resp = std::move(*intercepted);
		} else {
			resp = co_await handler(std::move(req));
		}
	} catch (...) {
		resp = response::server_error();
	}

	self->complete_request(sequence, std::move(resp));
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::complete_request(std::size_t sequence, http::response response) {
	if (shutdown_started_)
		return;

	auto ctx_it = request_ctxs_.find(sequence);
	if (ctx_it == request_ctxs_.end())
		return logger_.error("error in coroutine_http_session during on_handler_complete{{req_ctx.find}}: {}",
		                     "context could not be found in session map on completion");

	if (!close_policy_.should_drop_response(ctx_it->second)) {
		try {
			resp_interceptor_chain_.run(response);
		} catch (...) {
			response = http::response::server_error();
		}
	}

	const auto [drop_response, close_after_write] = close_policy_.on_response_ready(ctx_it->second, response);
	if (!close_policy_.accepting_requests())
		stop_reading_ = true;

	if (drop_response) {
		finish_request(sequence);
		if (outstanding_requests_ == 0 && stop_reading_)
			graceful_shutdown();
		return;
	}

	response.prepare_payload();
	pending_responses_.emplace(sequence,
	                           pending_write {.response = std::move(response), .close_after_write = close_after_write});
	notify_write_loop();
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::graceful_shutdown() {
	if (shutdown_started_)
		return;
	shutdown_started_ = true;
	read_signal_.cancel();
	write_signal_.cancel();
	Transport::graceful_shutdown(*this);
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::abort_transport() {
	if (shutdown_started_)
		return;
	shutdown_started_ = true;
	read_signal_.cancel();
	write_signal_.cancel();
	Transport::abort(stream_);
}

template <warp_session_transport Transport>
void coroutine_http_session<Transport>::finish_request(std::size_t sequence) {
	request_ctxs_.erase(sequence);
	--outstanding_requests_;
}

} // namespace warp::server
