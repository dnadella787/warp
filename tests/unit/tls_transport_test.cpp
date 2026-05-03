#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>

#include "server/session/policy/transport.h"
#include "ssl/ssl_context_provider.h"
#include "warp/logging/logger.hpp"
#include "warp/ssl/file_cert_loader.hpp"
#include "warp/ssl/ssl_config.hpp"

namespace warp::tests {

namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

namespace {

std::string test_source_path(std::string_view relative_path) {
	return std::string(WARP_TEST_SOURCE_DIR) + "/" + std::string(relative_path);
}

std::string tls_fixture_path(std::string_view filename) {
	return std::string(WARP_TEST_TLS_FIXTURE_DIR) + "/" + std::string(filename);
}

void run_io_for(asio::io_context &ioc, std::chrono::milliseconds duration) {
	ioc.restart();
	asio::steady_timer stop_timer(ioc);
	stop_timer.expires_after(duration);
	stop_timer.async_wait([&ioc](const boost::system::error_code &) { ioc.stop(); });
	ioc.run();
}

class tls_shutdown_test_session : public std::enable_shared_from_this<tls_shutdown_test_session> {
public:
	explicit tls_shutdown_test_session(tcp::socket &&socket, const std::shared_ptr<ssl::context> &server_context)
	    : stream_(std::move(socket), *server_context), logger_(log::logger::default_logger()) {
	}

	void start() {
	}

	void dispatch_sync_handler(std::size_t, const http::sync_handler &, http::request) {
	}

	void dispatch_async_handler(std::size_t, const http::async_handler &, http::request) {
	}

	server::tls_session_transport::stream_type stream_;
	log::logger logger_;
};

} // namespace

} // namespace warp::tests

namespace warp::server {

template <>
struct http_session_traits<warp::tests::tls_shutdown_test_session> {
	using transport_type = tls_session_transport;
	static constexpr auto event_loop_mode = http::event_loop_mode::callbacks;
};

} // namespace warp::server

namespace warp::tests {

TEST(TlsTransportTest, GracefulShutdownTimesOutAndReleasesSessionWhenPeerWithholdsCloseNotify) {
	asio::io_context server_ioc;
	asio::io_context client_ioc;

	server::ssl_context_provider context_provider(
	    warp::ssl::ssl_config(true, warp::ssl::file_cert_loader(tls_fixture_path("test_server_identity.pem"))));
	const auto server_context = context_provider.current();

	ssl::context client_context(ssl::context::tls_client);
	client_context.set_verify_mode(ssl::verify_none);

	tcp::acceptor acceptor(server_ioc, tcp::endpoint {tcp::v4(), 0});
	const auto endpoint = tcp::endpoint {asio::ip::make_address("127.0.0.1"), acceptor.local_endpoint().port()};

	ssl::stream<beast::tcp_stream> client_stream(client_ioc, client_context);
	beast::get_lowest_layer(client_stream).connect(endpoint);

	auto session = std::make_shared<tls_shutdown_test_session>(acceptor.accept(server_ioc), server_context);

	std::exception_ptr server_handshake_error;
	std::thread server_handshake_thread([&] {
		try {
			session->stream_.handshake(ssl::stream_base::server);
		} catch (...) {
			server_handshake_error = std::current_exception();
		}
	});
	ASSERT_NO_THROW(client_stream.handshake(ssl::stream_base::client));
	server_handshake_thread.join();
	ASSERT_EQ(server_handshake_error, nullptr);

	ASSERT_NO_THROW(asio::write(session->stream_, asio::buffer("ok", 2)));
	std::array<char, 2> payload {};
	ASSERT_NO_THROW(asio::read(client_stream, asio::buffer(payload)));
	EXPECT_EQ(std::string_view(payload.data(), payload.size()), "ok");

	std::weak_ptr<tls_shutdown_test_session> weak_session = session;
	server::tls_session_transport::graceful_shutdown(*session);
	session.reset();

	run_io_for(server_ioc, 50ms);
	EXPECT_FALSE(weak_session.expired());

	run_io_for(server_ioc, 6s);
	EXPECT_TRUE(weak_session.expired());
}

TEST(TlsTransportTest, StreamTruncatedIsTreatedAsGracefulTlsReadClosure) {
	EXPECT_TRUE(server::tls_session_transport::should_treat_read_error_as_eof(ssl::error::stream_truncated));
	EXPECT_FALSE(server::plain_session_transport::should_treat_read_error_as_eof(ssl::error::stream_truncated));
}

} // namespace warp::tests
