#include "coroutine_http_session.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http.hpp>

#include "callback_http_session.hpp"
#include "../../common/util/fail.h"
#include "../../common/util/lambda.h"

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

namespace warp::http {

coroutine_http_session::coroutine_http_session(boost::asio::ip::tcp::socket &&socket, registry &routes)
    : stream_(std::move(socket)), routes_(routes), read_signal_(stream_.get_executor()),
      write_signal_(stream_.get_executor()) {
}

void coroutine_http_session::start() {
	boost::asio::co_spawn(
	    stream_.get_executor(),
	    [self = shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->write_loop(); },
	    boost::asio::detached);

	boost::asio::co_spawn(
	    stream_.get_executor(),
	    [self = shared_from_this()]() -> boost::asio::awaitable<void> { co_await self->read_loop(); },
	    boost::asio::detached);
}

boost::asio::awaitable<void> coroutine_http_session::read_loop() {
	for (;;) {
		while (!shutdown_started_ && !stop_reading_ && outstanding_requests_ >= pipeline_limit_) {
			co_await wait_for_read_ready();
		}

		if (shutdown_started_ || stop_reading_) {
			if (stop_reading_ && outstanding_requests_ == 0) {
				shutdown();
			}
			co_return;
		}

		parser_.emplace();
		parser_->body_limit(10000);
		stream_.expires_after(std::chrono::seconds(30));

		beast::error_code ec;
		co_await beast::http::async_read(stream_, buffer_, *parser_,
		                                 boost::asio::redirect_error(boost::asio::use_awaitable, ec));

		if (ec == beast::http::error::end_of_stream) {
			stop_reading_ = true;
			if (outstanding_requests_ == 0)
				shutdown();
			co_return;
		}

		if (ec) {
			util::fail(ec, COMPONENT, "read_loop");
			co_return shutdown();
		}

		request req {parser_->release()};
		const auto sequence = next_request_sequence_++;
		request_ctxs_.emplace(
		    sequence,
		    request_context {.sequence = sequence, .version = req.version(), .client_keep_alive = req.keep_alive()});
		++outstanding_requests_;

		policy_.on_request_accepted(sequence, req.keep_alive());
		if (!policy_.accepting_requests())
			stop_reading_ = true;

		if (const auto *handler = routes_.find(req)) {
			std::visit(
			    common::overloaded {[&](const sync_handler &h) { execute_sync_handler(sequence, h, std::move(req)); },
			                        [&](const async_handler &h) {
				                        boost::asio::co_spawn(stream_.get_executor(),
				                                              execute_async_handler(sequence, h, std::move(req)),
				                                              boost::asio::detached);
			                        }},
			    *handler);
		} else {
			complete_request(sequence, response::not_found());
		}
	}
}

boost::asio::awaitable<void> coroutine_http_session::write_loop() {
	for (;;) {
		std::map<std::size_t, pending_write>::iterator it;
		while (!shutdown_started_) {
			it = pending_responses_.find(next_write_sequence_);
			if (it != pending_responses_.end())
				break;
			if (stop_reading_ && outstanding_requests_ == 0)
				co_return shutdown();
			co_await wait_for_write_ready();
		}

		if (shutdown_started_)
			co_return;

		stream_.expires_after(std::chrono::seconds(30));
		beast::error_code ec;
		co_await beast::http::async_write(stream_, it->second.response,
		                                  boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			util::fail(ec, COMPONENT, "write_loop");
			co_return shutdown();
		}

		bool close_after_write = it->second.close_after_write;
		pending_responses_.erase(it);
		finish_request(next_write_sequence_++);
		if (close_after_write)
			co_return shutdown();

		if (stop_reading_ && outstanding_requests_ == 0)
			co_return shutdown();

		notify_read_loop();
	}
}

boost::asio::awaitable<void> coroutine_http_session::wait_for_read_ready() {
	read_signal_.expires_at(boost::asio::steady_timer::time_point::max());
	beast::error_code ec;
	co_await read_signal_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
}

boost::asio::awaitable<void> coroutine_http_session::wait_for_write_ready() {
	write_signal_.expires_at(boost::asio::steady_timer::time_point::max());
	beast::error_code ec;
	co_await write_signal_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
}

void coroutine_http_session::execute_sync_handler(std::size_t sequence, const sync_handler &handler, request req) {
	try {
		auto resp = handler(std::move(req));
		complete_request(sequence, std::move(resp));
	} catch (...) {
		complete_request(sequence, response::server_error());
	}
}

boost::asio::awaitable<void> coroutine_http_session::execute_async_handler(std::size_t sequence,
                                                                           const async_handler &handler, request req) {
	try {
		auto resp = co_await handler(std::move(req));
		complete_request(sequence, std::move(resp));
	} catch (...) {
		complete_request(sequence, response::server_error());
	}
}

void coroutine_http_session::complete_request(std::size_t sequence, response response) {
	if (shutdown_started_)
		return;

	auto ctx_it = request_ctxs_.find(sequence);
	if (ctx_it == request_ctxs_.end())
		return util::fail(COMPONENT, "on_handler_complete{req_ctx.find}",
		                  "context could not be found in session map on completion");

	auto decision = policy_.on_response_ready(ctx_it->second, response);
	if (!policy_.accepting_requests())
		stop_reading_ = true;

	if (decision.drop_response) {
		finish_request(sequence);
		if (outstanding_requests_ == 0 && stop_reading_)
			shutdown();
		return;
	}

	response.prepare_payload();
	pending_responses_.emplace(
	    sequence, pending_write {.response = std::move(response), .close_after_write = decision.close_after_write});
	notify_write_loop();
}

void coroutine_http_session::notify_read_loop() {
	read_signal_.cancel();
}

void coroutine_http_session::notify_write_loop() {
	write_signal_.cancel();
}

void coroutine_http_session::shutdown() {
	if (shutdown_started_) {
		return;
	}
	shutdown_started_ = true;
	read_signal_.cancel();
	write_signal_.cancel();

	boost::system::error_code ec;
	stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
	if (ec && ec != boost::asio::error::not_connected) {
		util::fail(ec, COMPONENT, "shutdown");
	}
}

void coroutine_http_session::finish_request(std::size_t sequence) {
	request_ctxs_.erase(sequence);
	--outstanding_requests_;
}

} // namespace warp::http
