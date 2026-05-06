#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "interceptors/interceptor_chain.h"
#include "job/job_manager.h"
#include "listener/base_listener.hpp"
#include "listener/traits.hpp"
#include "router/route_runtime.hpp"
#include "session/callback_http_session.hpp"
#include "session/coroutine_http_session.hpp"
#include "session/policy/transport.h"
#include "session/policy/transport_provider.h"
#include "warp/logging/logger.hpp"
#include "warp/server/detail/route_definition.hpp"
#include "warp/server/server.hpp"

namespace warp::server {

template <http::event_loop_mode Mode>
class server::server_impl : public impl_base {
public:
	server_impl(const std::string &address, std::uint16_t port, std::size_t workers,
	            std::vector<detail::route_definition> route_definitions, warp::ssl::ssl_config ssl_config,
	            std::vector<job::background_job> jobs,
	            std::vector<detail::interceptor_definition<detail::type_erased_req_interceptor>> req_interceptors,
	            std::vector<detail::interceptor_definition<detail::type_erased_resp_interceptor>> resp_interceptors,
	            log::logger logger);

	void run(bool blocking) override;
	void stop() override;

private:
	friend struct detail::server_test_access;

	enum class lifecycle_state {
		stopped,
		starting,
		running,
		stopping,
	};

	void start_runner_threads();
	std::vector<std::thread> stop_io_ctx();
	static void join_runner_threads(std::vector<std::thread> threads, std::thread::id current_thread_id);
	template <warp_session_transport Transport>
	using session_t = typename event_loop_traits<Mode, Transport>::session_type;
	template <warp_session_transport Transport>
	using route_runtime_t = route_runtime<session_t<Transport>>;
	using plain_runtime_t = route_runtime_t<plain_session_transport>;
	using tls_runtime_t = route_runtime_t<tls_session_transport>;
	using route_runtime_variant_t = std::variant<plain_runtime_t, tls_runtime_t>;

	[[nodiscard]] std::shared_ptr<base_listener> make_listener(warp::ssl::ssl_config ssl_config);
	[[nodiscard]] static route_runtime_variant_t
	make_route_runtime(std::vector<detail::route_definition> route_definitions, bool tls_enabled);
	template <warp_session_transport Transport>
	[[nodiscard]] std::shared_ptr<base_listener> make_typed_listener(const route_runtime_t<Transport> &routes,
	                                                                 warp::ssl::ssl_config ssl_config = {});

	std::size_t pool_size_;
	boost::asio::io_context io_ctx_;
	std::string address_;
	std::uint16_t port_;
	job_manager jobs_;
	interceptor_chain<request> req_interceptor_chain_;
	interceptor_chain<response> resp_interceptor_chain_;
	log::logger logger_;
	route_runtime_variant_t route_runtime_;
	std::shared_ptr<base_listener> listener_;
	std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guard_;
	std::vector<std::thread> threads_;
	std::mutex lifecycle_mutex_;
	std::condition_variable lifecycle_cv_;
	lifecycle_state state_ {lifecycle_state::stopped};
	std::optional<std::thread::id> stopping_thread_id_;

	template <warp_session_transport Transport>
	using listener_t = typename event_loop_traits<Mode, Transport>::listener_type;
};

} // namespace warp::server

#include "server_impl.tpp"
