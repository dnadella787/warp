#include "server_impl.hpp"

#include <iostream>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "session.hpp"
#include "warp/http/server.hpp"

namespace warp::http {

server::impl::impl(const std::string& address, unsigned short port, std::size_t workers, const net::router::registry& routes)
	: io_ctx_(static_cast<int>(pool_size_)), listener_(io_ctx_, address, port),
      guard_(boost::asio::make_work_guard(io_ctx_)), pool_size_(workers ? workers : 1),
	   routes_(routes) {
	threads_.reserve(pool_size_);
}

/*
 std::atomic<bool> running_ checked via std::atomic::exchange(true/false, std::memory_order_acq_rel)
 guarantees that run()/stop() check the value and update the server resources (like thread pool/acceptor)
 accordingly before propagating the change downstream to an acquire on another thread (e.g. run()/stop()
 or do_accept()).

 If thread A calls stop() w/ acq_rel and thread B calls do_accept with acquire. The compiler
 is forbidden from moving something like acceptor->close() to after running is set to false b/c
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
	io_ctx_.run();
	start_runner_threads();
	do_accept();
	running_.store(true);
}

void server::impl::stop() {
	// try to stop, if its already stopped just return early, use acq_rel
	// b/c we want to ensure that all previous activity on running_ is
	// published (i.e. from run() or other stop() threads) before we cancel
	// the io_context pool and acceptor
	if (!running_.exchange(false, std::memory_order_acq_rel)) {
		return;
	}

	guard_.reset();
	io_ctx_.stop();
	for (auto &t : threads_) {
		if (t.joinable()) {
			t.join();
		}
	}
	threads_.clear();
	running_.store(false);
}

void server::impl::do_accept() {
	// check if server is running, if it is, then:
	// 1. start the event loop and asynchronously wait for new TCP connection
	// 2. create a session that consumes the socket and path/handler registry
	// 3. start the session (which parses the buffer from the socket, maps it to a
	//    handler, executes the handler, and ultimately returns a response back on
	//    the same socket (the strand is created at the session level and the tcp_stream
	//    is directly bound to it so the events for a request execute serially)
	// 4. accept the next connection
	//
	// do_accept() is read only on the running_ flag which is why it
	// only needs to acquire the value from producers that r/w like
	// run() and stop()
	if (!running_.load(std::memory_order_acquire)) {
		return;
	}
	acceptor_->async_accept(
	    [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
		    if (!ec) {
			    std::make_shared<detail::session>(std::move(socket), self->routes_)->start();
		    }
		    self->do_accept();
	    });
}

void server::impl::start_runner_threads() {
	for (std::size_t i = 0; i < pool_size_; i++) {
		threads_.emplace_back([&ctx = io_ctx_]() {
			for (;;) {
				try {
					ctx.run();
					break;
				} catch (const std::exception &ex) {
					std::cerr << "worker error: " << ex.what() << std::endl;
				}
			}
		});
	}
}

void server::impl::stop_runner_threads() {
}

} // namespace warp::http
