#pragma once

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

#include "server/router/registry.hpp"
#include "warp/http/event_loop_mode.hpp"
#include "warp/http/http.hpp"

namespace warp::server {

class callback_http_session;
class coroutine_http_session;

template <http::event_loop_mode Mode>
class route_executor_table;

namespace detail {

struct callback_route_executor {
	using dispatch_fn = void (*)(callback_http_session &, std::size_t, http::request, const callback_route_executor &);

	dispatch_fn dispatch {};
	std::variant<http::sync_handler, http::async_handler> handler;

	void invoke(callback_http_session &session, std::size_t sequence, http::request request) const {
		dispatch(session, sequence, std::move(request), *this);
	}
};

struct coroutine_route_executor {
	using dispatch_fn = void (*)(coroutine_http_session &, std::size_t, http::request,
	                             const coroutine_route_executor &);

	dispatch_fn dispatch {};
	std::variant<http::sync_handler, http::async_handler> handler;

	void invoke(coroutine_http_session &session, std::size_t sequence, http::request request) const {
		dispatch(session, sequence, std::move(request), *this);
	}
};

[[nodiscard]] callback_route_executor make_callback_route_executor(http::handler handler);
[[nodiscard]] coroutine_route_executor make_coroutine_route_executor(http::handler handler);

} // namespace detail

template <>
class route_executor_table<http::event_loop_mode::callbacks> {
public:
	route_executor_table() = default;

	explicit route_executor_table(std::size_t count) : executors_(count) {
	}

	void set(registry::route_id id, http::handler handler);

	void dispatch(registry::route_id id, callback_http_session &session, std::size_t sequence,
	              http::request request) const {
		executors_[id.index()].invoke(session, sequence, std::move(request));
	}

	[[nodiscard]] bool empty() const noexcept {
		return executors_.empty();
	}

private:
	std::vector<detail::callback_route_executor> executors_;
};

template <>
class route_executor_table<http::event_loop_mode::coroutines> {
public:
	route_executor_table() = default;

	explicit route_executor_table(std::size_t count) : executors_(count) {
	}

	void set(registry::route_id id, http::handler handler);

	void dispatch(registry::route_id id, coroutine_http_session &session, std::size_t sequence,
	              http::request request) const {
		executors_[id.index()].invoke(session, sequence, std::move(request));
	}

	[[nodiscard]] bool empty() const noexcept {
		return executors_.empty();
	}

private:
	std::vector<detail::coroutine_route_executor> executors_;
};

} // namespace warp::server
