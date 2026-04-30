#pragma once
#include <boost/beast/core.hpp>

#include "http_listener.h"
#include "server/router/registry.hpp"

namespace warp::server {

class callback_listener final : public http_listener<callback_listener>,
                                public std::enable_shared_from_this<callback_listener> {
public:
	callback_listener(boost::asio::io_context &ioc, const registry &registry,
	                  const interceptor_chain &interceptor_chain, const std::string &address, unsigned short port,
	                  log::logger logger);

	void execute();

private:
	void do_accept();
	void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
};

} // namespace warp::server
