#pragma once

#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <pqxx/connection>

#include "warp/db/postgres/connection_pool.hpp"

namespace warp::db::postgres {

class connection_pool::impl : public std::enable_shared_from_this<impl> {
public:
	impl(executor_type completion_executor, connection_config cfg, std::size_t max_connections,
	     std::size_t worker_threads);
	~impl();

	void close();
	[[nodiscard]] std::size_t size() const;
	[[nodiscard]] std::size_t capacity() const noexcept;

	result sync_query(std::string sql);

	template <typename CompletionToken>
	auto async_query(std::string sql, CompletionToken &&token) {
		return boost::asio::async_initiate<CompletionToken, void(result)>(
		    [self = shared_from_this(), sql = std::move(sql)](auto &&handler) mutable {
			    using Handler = std::decay_t<decltype(handler)>;
			    auto handler_ptr = std::make_shared<Handler>(std::forward<decltype(handler)>(handler));
			    auto allocator = boost::asio::get_associated_allocator(*handler_ptr);
			    auto executor = boost::asio::get_associated_executor(*handler_ptr, self->completion_executor_);
			    boost::asio::post(
			        self->workers_, [self, sql = std::move(sql), handler_ptr, executor, allocator]() mutable {
				        std::exception_ptr eptr;
				        result res;
				        try {
					        res = self->execute_query(std::move(sql));
				        } catch (...) {
					        eptr = std::current_exception();
				        }
				        boost::asio::dispatch(boost::asio::bind_allocator(
				            allocator,
				            boost::asio::bind_executor(executor, [handler_ptr, eptr, res = std::move(res)]() mutable {
					            if (eptr) {
						            std::rethrow_exception(eptr);
					            }
					            (*handler_ptr)(std::move(res));
				            })));
			        });
		    },
		    std::forward<CompletionToken>(token));
	}

private:
	std::unique_ptr<pqxx::connection> acquire();
	void release(std::unique_ptr<pqxx::connection> conn);
	void discard(std::unique_ptr<pqxx::connection> conn);
	result execute_query(std::string sql);

	executor_type completion_executor_;
	connection_config config_;
	std::size_t capacity_;
	boost::asio::thread_pool workers_;
	mutable std::mutex mutex_;
	std::condition_variable cond_;
	std::deque<std::unique_ptr<pqxx::connection>> idle_;
	std::size_t total_ {0};
	bool closed_ {false};
};

} // namespace warp::db::postgres
