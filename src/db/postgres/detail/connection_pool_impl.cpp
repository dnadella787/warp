#include "connection_pool_impl.hpp"

#include <algorithm>
#include <future>
#include <pqxx/pqxx>
#include <stdexcept>
#include <utility>

namespace warp::db::postgres {

connection_pool::impl::impl(executor_type completion_executor, connection_config cfg, std::size_t max_connections,
    std::size_t worker_threads)
    : completion_executor_(std::move(completion_executor))
    , config_(std::move(cfg))
    , capacity_(std::max<std::size_t>(1, max_connections))
    , workers_(std::max<std::size_t>(1, worker_threads)) {}

connection_pool::impl::~impl() {
	close();
	workers_.join();
}

void connection_pool::impl::close() {
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

std::size_t connection_pool::impl::size() const {
	std::lock_guard lock(mutex_);
	return total_;
}

std::size_t connection_pool::impl::capacity() const noexcept {
	return capacity_;
}

result connection_pool::impl::sync_query(std::string sql) {
	auto task = std::make_shared<std::packaged_task<result()>>(
	    [self = shared_from_this(), sql = std::move(sql)]() mutable {
		    return self->execute_query(std::move(sql));
	    });
	auto future = task->get_future();
	boost::asio::post(workers_, [task]() mutable { (*task)(); });
	return future.get();
}

std::unique_ptr<pqxx::connection> connection_pool::impl::acquire() {
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

void connection_pool::impl::release(std::unique_ptr<pqxx::connection> conn) {
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

void connection_pool::impl::discard(std::unique_ptr<pqxx::connection> conn) {
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

result connection_pool::impl::execute_query(std::string sql) {
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

} // namespace warp::db::postgres
