#pragma once

#include <memory>
#include <string>
#include <thread>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>

#include "connection_config.hpp"
#include "result.hpp"

namespace warp::db::postgres {

class connection_pool {
public:
	using executor_type = boost::asio::any_io_executor;

	connection_pool(executor_type completion_executor, connection_config config, std::size_t max_connections = 8,
	                std::size_t worker_threads = std::thread::hardware_concurrency());
	connection_pool(connection_pool &&) noexcept;
	connection_pool &operator=(connection_pool &&) noexcept;
	connection_pool(const connection_pool &) = delete;
	connection_pool &operator=(const connection_pool &) = delete;
	~connection_pool();

	result query(std::string sql) const;
	boost::asio::awaitable<result> async_query(std::string sql) const;

	void close();
	[[nodiscard]] std::size_t size() const noexcept;
	[[nodiscard]] std::size_t capacity() const noexcept;

private:
	class impl;
	std::shared_ptr<impl> impl_ {};
};

} // namespace warp::db::postgres
