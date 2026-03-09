#include "warp/db/postgres/connection_pool.hpp"

#include <boost/asio/use_awaitable.hpp>

#include <stdexcept>
#include <utility>

#include "detail/connection_pool_impl.hpp"

namespace warp::db::postgres {

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
	co_return co_await impl_->async_query(std::move(sql), boost::asio::use_awaitable);
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
