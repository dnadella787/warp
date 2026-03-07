#include "server_impl.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "session.hpp"
#include "warp/http/server.hpp"
#include "../../net/core/io_context_pool.hpp"

namespace warp::http {

server::impl::impl(std::string address, std::uint16_t port, std::size_t workers, net::router::registry routes)
    : address_(std::move(address)), port_(port), pool_(workers),
      accept_ctx_(std::make_shared<boost::asio::io_context>()), routes_(routes) {
	boost::asio::ip::tcp::resolver resolver(*accept_ctx_);
	auto endpoints = resolver.resolve(address_, std::to_string(port_));
	acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(*accept_ctx_);
	boost::asio::ip::tcp::endpoint endpoint = *endpoints.begin();
	acceptor_->open(endpoint.protocol());
	acceptor_->set_option(boost::asio::socket_base::reuse_address(true));
	acceptor_->bind(endpoint);
	acceptor_->listen();
}

/*
 std::atomic<bool> running_ checked via exchange using std::memory_order_acq_rel guarantees
 that run()/stop() check the value and update the server resources (like thread pool/acceptor)
 accordingly before propagating the change downstream to another acquire (like another thread's
 run()/stop() or do_accept() which only acquires).

 If thread A calls stop() w/ acq_rel and thread B calls do_accept with acquire. The compiler
 is forbidden from moving something acceptor->close() to after running is set to false b/c
 the compiler will enforce a happens-before relationship between stop and do_accept across
 the threads

 TLDR: run/stop are r/w, do_accept is r only
 */

void server::impl::run() {
	// try to start, if its already running just return early, use acq_rel
	// b/c we want to acquire the current state and check if it in a non-running
	// state, and then release it to consumers like do_accept()/stop()
	if (running_.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	do_accept();
	pool_.run();
	accept_ctx_->run();
	running_.store(false);
}

void server::impl::stop() {
	// try to stop, if its already stopped just return early, use acq_rel
	// b/c we want to ensure that all previous activity on running_ is
	// published (i.e. from run() or other stop() threads) before we cancel
	// the thread pool and acceptor
	if (!running_.exchange(false, std::memory_order_acq_rel)) {
		return;
	}
	boost::asio::dispatch(*accept_ctx_, [acceptor = acceptor_.get()]() {
		if (!acceptor) {
			return;
		}
		boost::system::error_code ec;
		acceptor->cancel(ec);
		acceptor->close(ec);
	});
	accept_ctx_->stop();
	pool_.stop();
}

void server::impl::do_accept() {
	// check if server is running, if it is, then ...
	// do_accept() is read only on the running_ flag which is why it
	// only needs to acquire the value from producers that r/w like
	// run() and stop()
	if (!running_.load(std::memory_order_acquire)) {
		return;
	}
	acceptor_->async_accept(
	    boost::asio::make_strand(pool_.next()),
	    [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
		    if (!ec) {
			    std::make_shared<detail::session>(std::move(socket), self->routes_)->start();
		    }
		    self->do_accept();
	    });
}

std::uint16_t server::impl::port() const {
	boost::system::error_code ec;
	auto ep = acceptor_ ? acceptor_->local_endpoint(ec) : boost::asio::ip::tcp::endpoint {};
	if (ec) {
		return 0;
	}
	return ep.port();
}

} // namespace warp::http
