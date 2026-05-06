#pragma once
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>

#include "http_listener.h"

namespace warp::server {

template <warp_session_transport Transport>
class callback_listener final : public listener_base<event_loop_mode::callbacks, Transport>,
                                public std::enable_shared_from_this<callback_listener<Transport>> {
public:
	using base_type = listener_base<event_loop_mode::callbacks, Transport>;

	callback_listener(boost::asio::io_context &ioc, transport_provider<Transport> transport_provider,
	                  const typename base_type::route_runtime_t &routes,
	                  const interceptor_chain<request> &req_interceptor_chain,
	                  const interceptor_chain<response> &resp_interceptor_chain, const std::string &address,
	                  unsigned short port, log::logger logger);

	void run() override;

private:
	void do_accept();
	void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
};

} // namespace warp::server
