#pragma once

#include <concepts>
#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

#include "server/router/registry.hpp"
#include "warp/http/http.hpp"

namespace warp::server {

template <typename Session>
class route_executor_table {
public:
	using dispatch_fn = void (*)(Session &, std::size_t, http::request, const http::handler &);

	route_executor_table() = default;

	explicit route_executor_table(std::size_t count) : executors_(count) {
	}

	void set(registry::route_id id, http::handler handler) {
		if (executors_.size() <= id.index()) {
			executors_.resize(id.index() + 1);
		}

		executors_[id.index()] = std::move(make_route_executor(handler));
	}

	void dispatch(registry::route_id id, Session &session, std::size_t sequence, http::request request) const {
		executors_[id.index()].invoke(session, sequence, std::move(request));
	}

	[[nodiscard]] bool empty() const noexcept {
		return executors_.empty();
	}

private:
	struct route_executor {
		using invoke_fn = std::function<void(Session &, std::size_t, http::request)>;

		invoke_fn invoke_impl;

		void invoke(Session &session, std::size_t sequence, http::request request) const {
			invoke_impl(session, sequence, std::move(request));
		}
	};

	static route_executor make_sync_executor(http::sync_handler handler) {
		return route_executor {
		    [handler = std::move(handler)](Session &session, std::size_t sequence, http::request request) mutable {
			    session.dispatch_sync_handler(sequence, handler, std::move(request));
		    }};
	}

	static route_executor make_async_executor(http::async_handler handler) {
		return route_executor {
		    [handler = std::move(handler)](Session &session, std::size_t sequence, http::request request) mutable {
			    session.dispatch_async_handler(sequence, handler, std::move(request));
		    }};
	}

	route_executor make_route_executor(http::handler handler) {
		if (auto *sync = std::get_if<http::sync_handler>(&handler))
			return make_sync_executor(std::move(*sync));

		auto *async = std::get_if<http::async_handler>(&handler);
		return make_async_executor(std::move(*async));
	}

	std::vector<route_executor> executors_;
};

} // namespace warp::server
