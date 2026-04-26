#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "listener/base_listener.hpp"
#include "listener/traits.hpp"
#include "router/registry.hpp"
#include "warp/server/server.hpp"

namespace warp::server {

template <http::event_loop_mode Mode>
class server::server_impl : public impl_base {
public:
	server_impl(const std::string &address, std::uint16_t port, std::size_t workers, registry routes);

	void run(bool blocking) override;
	void stop() override;

private:
	enum class lifecycle_state {
		stopped,
		starting,
		running,
		stopping,
	};

	void start_runner_threads();
	std::vector<std::thread> stop_io_ctx();
	static void join_runner_threads(std::vector<std::thread> threads, std::thread::id current_thread_id);

	std::size_t pool_size_;
	boost::asio::io_context io_ctx_;
	registry routes_;
	std::shared_ptr<base_listener> listener_;
	std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guard_;
	std::vector<std::thread> threads_;
	std::mutex lifecycle_mutex_;
	std::condition_variable lifecycle_cv_;
	lifecycle_state state_ {lifecycle_state::stopped};
	std::optional<std::thread::id> stopping_thread_id_;

	using listener_t = listener_traits<Mode>::type;
};

} // namespace warp::server

#include "server_impl.tpp"
