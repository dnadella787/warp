#pragma once
#include <boost/beast/core.hpp>

#include "http_listener.h"
#include "../router/registry.hpp"

namespace warp::http {

class callback_listener final : public http_listener<callback_listener>,
                                public std::enable_shared_from_this<callback_listener> {
public:
	callback_listener(boost::asio::io_context &ioc, registry &registry, const std::string &address,
	                  unsigned short port);

	void execute();

private:
	void do_accept();
	void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

	static constexpr std::string_view COMPONENT {"listener"};
};

} // namespace warp::http
