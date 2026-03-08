#include "warp/db/postgres/connection_pool.hpp"

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <future>
#include <mutex>
#include <pqxx/pqxx>
#include <stdexcept>
#include <utility>

namespace warp::db::postgres {

struct connection_pool::impl : public std::enable_shared_from_this<connection_pool::impl> {
	using executor_type = connection_pool::executor_type;

	impl(executor_type completion_executor, connection_config cfg, std::size_t max_connections,
	     std::size_t worker_threads)
	    : completion_executor_(std::move(completion_executor)), config_(std::move(cfg)),
	      capacity_(std::max<std::size_t>(1, max_connections)), workers_(std::max<std::size_t>(1, worker_threads)) {
	}

	~impl() {
		close();
		workers_.join();
	}

	void close() {
		std::deque<std::unique_ptr<pqxx::connection>> to_close;
		{
			std::lock_guard lock(mutex_);
			if (closed_) {
				return;
			}
			closed_ = true;
			to_close.swap(idle_);
		}
		for (auto &conn : to_close) {
			if (!conn) {
				continue;
			}
			try {
				conn.reset();
			} catch (...) {
			}
		}
		cond_.notify_all();
	}

	std::size_t size() const {
		std::lock_guard lock(mutex_);
		return total_;
	}

	std::size_t capacity() const noexcept {
		return capacity_;
	}

	result sync_query(std::string sql) {
		auto task =
		    std::make_shared<std::packaged_task<result()>>([self = shared_from_this(), sql = std::move(sql)]() mutable {
			    return self->execute_query(std::move(sql));
		    });
		auto future = task->get_future();
		boost::asio::post(workers_, [task]() mutable { (*task)(); });
		return future.get();
	}

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
	std::unique_ptr<pqxx::connection> acquire() {
		std::unique_lock lock(mutex_);
		cond_.wait(lock, [this]() { return closed_ || !idle_.empty() || total_ < capacity_; });
		if (closed_) {
			throw std::runtime_error("connection pool is closed");
		}
		if (!idle_.empty()) {
			auto conn = std::move(idle_.back());
			idle_.pop_back();
			return conn;
		}
		++total_;
		lock.unlock();
		try {
			auto conn = std::make_unique<pqxx::connection>(config_.to_connection_string());
			return conn;
		} catch (...) {
			lock.lock();
			if (total_ > 0) {
				--total_;
			}
			lock.unlock();
			cond_.notify_one();
			throw;
		}
	}

	void release(std::unique_ptr<pqxx::connection> conn) {
		if (!conn) {
			return;
		}
		std::lock_guard lock(mutex_);
		if (closed_) {
			if (total_ > 0) {
				--total_;
			}
			return;
		}
		idle_.push_back(std::move(conn));
		cond_.notify_one();
	}

	void discard(std::unique_ptr<pqxx::connection> conn) {
		if (conn) {
			try {
				conn.reset();
			} catch (...) {
			}
		}
		std::lock_guard lock(mutex_);
		if (total_ > 0) {
			--total_;
		}
		cond_.notify_one();
	}

	result execute_query(std::string sql) {
		auto conn = acquire();
		try {
			if (!conn->is_open()) {
				conn = std::make_unique<pqxx::connection>(config_.to_connection_string());
			}
			pqxx::work txn(*conn);
			auto pq_res = txn.exec(sql);
			txn.commit();
			auto shared = std::make_shared<pqxx::result>(std::move(pq_res));
			result wrapped {std::move(shared)};
			release(std::move(conn));
			return wrapped;
		} catch (...) {
			discard(std::move(conn));
			throw;
		}
	}

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

connection_pool::connection_pool(executor_type completion_executor, connection_config config,
                                 std::size_t max_connections, std::size_t worker_threads)
    : impl_(
          std::make_shared<impl>(std::move(completion_executor), std::move(config), max_connections, worker_threads)) {
}

connection_pool::connection_pool(connection_pool &&) noexcept = default;
connection_pool &connection_pool::operator=(connection_pool &&) noexcept = default;

connection_pool::~connection_pool() {
	close();
}

result connection_pool::query(std::string sql) {
	if (!impl_) {
		throw std::runtime_error("connection pool not initialised");
	}
	return impl_->sync_query(std::move(sql));
}

boost::asio::awaitable<result> connection_pool::async_query(std::string sql) {
	if (!impl_) {
		throw std::runtime_error("connection pool not initialised");
	}
	auto res = co_await impl_->async_query(std::move(sql), boost::asio::use_awaitable);
	co_return res;
}

void connection_pool::close() {
	if (impl_) {
		impl_->close();
	}
}

std::size_t connection_pool::size() const noexcept {
	return impl_ ? impl_->size() : 0;
}

std::size_t connection_pool::capacity() const noexcept {
	return impl_ ? impl_->capacity() : 0;
}

} // namespace warp::db::postgres
