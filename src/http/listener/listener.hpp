#pragma once
#include <boost/beast/core.hpp>

#include "listener_base.hpp"
#include "../router/registry.hpp"

namespace warp::http {

class listener final : public listener_base, public std::enable_shared_from_this<listener> {
public:
	listener(boost::asio::io_context &ioc, registry &registry, const std::string &address, unsigned short port);

	void run() override;

private:
	void do_accept();
	void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

	boost::asio::io_context &ioc_;
	boost::asio::ip::tcp::acceptor acceptor_;
	registry &registry_;

	static constexpr std::string COMPONENT {"listener"};
};

} // namespace warp::http
