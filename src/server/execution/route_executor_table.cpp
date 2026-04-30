#include "route_executor_table.hpp"

#include <utility>

#include "server/session/callback_http_session.hpp"
#include "server/session/coroutine_http_session.hpp"

namespace warp::server {

namespace {

void dispatch_callback_sync(callback_http_session &session, std::size_t sequence, http::request request,
                            const detail::callback_route_executor &executor) {
	session.dispatch_sync_handler(sequence, std::get<http::sync_handler>(executor.handler), std::move(request));
}

void dispatch_callback_async(callback_http_session &session, std::size_t sequence, http::request request,
                             const detail::callback_route_executor &executor) {
	session.dispatch_async_handler(sequence, std::get<http::async_handler>(executor.handler), std::move(request));
}

void dispatch_coroutine_sync(coroutine_http_session &session, std::size_t sequence, http::request request,
                             const detail::coroutine_route_executor &executor) {
	session.dispatch_sync_handler(sequence, std::get<http::sync_handler>(executor.handler), std::move(request));
}

void dispatch_coroutine_async(coroutine_http_session &session, std::size_t sequence, http::request request,
                              const detail::coroutine_route_executor &executor) {
	session.dispatch_async_handler(sequence, std::get<http::async_handler>(executor.handler), std::move(request));
}

} // namespace

detail::callback_route_executor detail::make_callback_route_executor(http::handler handler) {
	callback_route_executor executor {.handler = std::move(handler)};
	if (std::holds_alternative<http::sync_handler>(executor.handler)) {
		executor.dispatch = &dispatch_callback_sync;
	} else {
		executor.dispatch = &dispatch_callback_async;
	}
	return executor;
}

detail::coroutine_route_executor detail::make_coroutine_route_executor(http::handler handler) {
	coroutine_route_executor executor {.handler = std::move(handler)};
	if (std::holds_alternative<http::sync_handler>(executor.handler)) {
		executor.dispatch = &dispatch_coroutine_sync;
	} else {
		executor.dispatch = &dispatch_coroutine_async;
	}
	return executor;
}

void route_executor_table<event_loop_mode::callbacks>::set(registry::route_id id, http::handler handler) {
	if (executors_.size() <= id.index()) {
		executors_.resize(id.index() + 1);
	}
	executors_[id.index()] = detail::make_callback_route_executor(std::move(handler));
}

void route_executor_table<event_loop_mode::coroutines>::set(registry::route_id id, http::handler handler) {
	if (executors_.size() <= id.index()) {
		executors_.resize(id.index() + 1);
	}
	executors_[id.index()] = detail::make_coroutine_route_executor(std::move(handler));
}

} // namespace warp::server
