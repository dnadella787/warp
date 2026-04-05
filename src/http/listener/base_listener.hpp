#pragma once

#include "../router/registry.hpp"
#include <boost/beast/core.hpp>

namespace warp::http {

class base_listener {
public:
	base_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address, unsigned short port);
	virtual ~base_listener() = default;
	virtual void run() = 0;

protected:
	boost::asio::io_context &ioc_;
	boost::asio::ip::tcp::acceptor acceptor_;
	registry &registry_;
};

} // namespace warp::http
