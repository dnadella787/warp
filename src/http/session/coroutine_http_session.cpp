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
			if (outstanding_requests_ == 0) {
				shutdown();
			}
			co_return;
		}

		if (ec) {
			util::fail(ec, COMPONENT, "read_loop");
			shutdown();
			co_return;
		}

		request req {parser_->release()};
		const auto sequence = next_request_sequence_++;
		const auto version = req.version();
		const auto keep_alive = req.keep_alive();
		++outstanding_requests_;

		if (!keep_alive) {
			stop_reading_ = true;
		}

		if (const auto *handler = routes_.find(req)) {
			std::visit(common::overloaded {[&](const sync_handler &h) {
				                               execute_sync_handler(sequence, version, keep_alive, h, std::move(req));
			                               },
			                               [&](const async_handler &h) {
				                               boost::asio::co_spawn(stream_.get_executor(),
				                                                     execute_async_handler(sequence, version,
				                                                                           keep_alive, h,
				                                                                           std::move(req)),
				                                                     boost::asio::detached);
			                               }},
			           *handler);
		} else {
			complete_request(sequence, version, keep_alive, response::not_found());
		}
	}
}

boost::asio::awaitable<void> coroutine_http_session::write_loop() {
	for (;;) {
		while (!shutdown_started_) {
			if (pending_responses_.find(next_write_sequence_) != pending_responses_.end()) {
				break;
			}
			if (stop_reading_ && outstanding_requests_ == 0) {
				shutdown();
				co_return;
			}
			co_await wait_for_write_ready();
		}

		if (shutdown_started_) {
			co_return;
		}

		const auto it = pending_responses_.find(next_write_sequence_);
		if (it == pending_responses_.end()) {
			continue;
		}

		stream_.expires_after(std::chrono::seconds(30));
		const auto keep_alive = it->second.keep_alive();
		beast::error_code ec;
		co_await beast::http::async_write(stream_, it->second,
		                                  boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			util::fail(ec, COMPONENT, "write_loop");
			shutdown();
			co_return;
		}

		pending_responses_.erase(it);
		if (outstanding_requests_ > 0) {
			--outstanding_requests_;
		}
		++next_write_sequence_;
		notify_read_loop();

		if (!keep_alive) {
			stop_reading_ = true;
			shutdown();
			co_return;
		}

		if (stop_reading_ && outstanding_requests_ == 0) {
			shutdown();
			co_return;
		}
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

void coroutine_http_session::execute_sync_handler(std::size_t sequence, unsigned version, bool keep_alive,
                                                  const sync_handler &handler, request req) {
	try {
		auto resp = handler(std::move(req));
		complete_request(sequence, version, keep_alive, std::move(resp));
	} catch (const std::exception &) {
		complete_request(sequence, version, keep_alive, response::server_error());
	}
}

boost::asio::awaitable<void> coroutine_http_session::execute_async_handler(std::size_t sequence, unsigned version,
                                                                           bool keep_alive,
                                                                           const async_handler &handler, request req) {
	try {
		auto resp = co_await handler(std::move(req));
		complete_request(sequence, version, keep_alive, std::move(resp));
	} catch (const std::exception &) {
		complete_request(sequence, version, keep_alive, response::server_error());
	}
}

void coroutine_http_session::complete_request(std::size_t sequence, unsigned version, bool keep_alive,
                                              response response) {
	if (shutdown_started_) {
		return;
	}

	response.version(version);
	response.keep_alive(keep_alive && response.keep_alive());
	response.prepare_payload();
	pending_responses_.emplace(sequence, std::move(response));
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

} // namespace warp::http
