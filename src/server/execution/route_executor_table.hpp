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

		auto &executor = executors_[id.index()];
		executor.handler = std::move(handler);
		executor.dispatch =
		    std::holds_alternative<http::sync_handler>(executor.handler) ? &dispatch_sync : &dispatch_async;
	}

	void dispatch(registry::route_id id, Session &session, std::size_t sequence, http::request request) const {
		executors_[id.index()].invoke(session, sequence, std::move(request));
	}

	[[nodiscard]] bool empty() const noexcept {
		return executors_.empty();
	}

private:
	struct route_executor {
		dispatch_fn dispatch {};
		http::handler handler;

		void invoke(Session &session, std::size_t sequence, http::request request) const {
			dispatch(session, sequence, std::move(request), handler);
		}
	};

	static void dispatch_sync(Session &session, std::size_t sequence, http::request request,
	                          const http::handler &handler) {
		session.dispatch_sync_handler(sequence, std::get<http::sync_handler>(handler), std::move(request));
	}

	static void dispatch_async(Session &session, std::size_t sequence, http::request request,
	                           const http::handler &handler) {
		session.dispatch_async_handler(sequence, std::get<http::async_handler>(handler), std::move(request));
	}

	std::vector<route_executor> executors_;
};

} // namespace warp::server
