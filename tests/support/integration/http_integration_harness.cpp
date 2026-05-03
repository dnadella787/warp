#include "http_integration_harness.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/json/parse.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "warp/server/server_builder.hpp"
#include "warp/ssl/file_cert_loader.hpp"

namespace warp::tests::integration_support {

using namespace std::chrono_literals;

namespace {

constexpr char kTestCertificatePem[] = R"(-----BEGIN CERTIFICATE-----
MIIDJTCCAg2gAwIBAgIUQiYFNSL4cmtZPBerXT/HBqxVTh0wDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI2MDUwMTA0NTU0M1oXDTI3MDUw
MTA0NTU0M1owFDESMBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAxttQULEB/58Ac3bP871CTnS24QDQbpU8dcEE1crF0fUZ
cmo8mXWHMwOZaCifC7rb+yEHNhRfNzIxPeEaXGelPevWYwxs36Cmc/EAF4as1kiC
zpKTbHCBFcenIdFri+pq0JiNER6N5Ps3Xiy2Xdm7huvV2gm1gmv87xb6nOXZyX70
PJQyJx7A90V8aO7GLgQ62rpVABsWANNH9rqWmUhIf+Xzc7fId7xNgR9cSS6rxdGS
E4IsMYjRko4ohd8TvT4EFTIriXcNY0UpBc7xWZLUnP9K2Ubuabwk0Sv0Wv93MN05
YcIZy6vkWNyYlBn5H/bkfq4hWCd9gwvL4l9NCzCkRwIDAQABo28wbTAdBgNVHQ4E
FgQUqDSzR7gj+NCle2K97LFVchJ5RrwwHwYDVR0jBBgwFoAUqDSzR7gj+NCle2K9
7LFVchJ5RrwwDwYDVR0TAQH/BAUwAwEB/zAaBgNVHREEEzARgglsb2NhbGhvc3SH
BH8AAAEwDQYJKoZIhvcNAQELBQADggEBALM5isM1EhmdiIM1lvg25CCawQNkLwdd
QJETb3AanQGM0dNEqux3gJBgf8sh2GR4ySd/4/A9UY3Oegxtnepa6w1v3EoaWf9N
5TWh/tam5jLJ/+U3zxSygbbL8Ybn14w1zZj4bD5cQJtrGYKDWnP2ovv2H5thNMZe
BRJ1G4zB1uT6rR0m1fIFadz+o0hRQbnWs4K5aZ6KwcYs8k3M9uvktpp6zgpGAnsc
dC/6x4DkATJEC6zgvdK3grWlw/6ArkWw9AfgmF6rMEuCHZHSODjlnaGc56mM0epo
avXC0pf+8Es9Duxxf5kFYHBHJftnhZzXMEyTXODffW/bnDkeKDnlw+U=
-----END CERTIFICATE-----
)";

std::string test_source_path(std::string_view relative_path) {
	return std::string(WARP_TEST_SOURCE_DIR) + "/" + std::string(relative_path);
}

template <typename Stream>
void write_payload(Stream &stream, std::string_view payload) {
	asio::write(stream, asio::buffer(payload.data(), payload.size()));
}

template <typename Stream>
http_response read_http_response(Stream &stream, beast::flat_buffer &buffer) {
	http_response response;
	http::read(stream, buffer, response);
	return response;
}

template <typename Stream>
bool read_http_eof(Stream &stream, beast::flat_buffer &buffer) {
	http_response response;
	beast::error_code ec;
	http::read(stream, buffer, response, ec);
	return ec == asio::error::eof || ec == beast::http::error::end_of_stream;
}

template <typename Stream>
bool drain_until_eof(Stream &stream) {
	std::array<char, 512> scratch {};
	beast::error_code ec;
	for (;;) {
		const auto bytes = stream.read_some(asio::buffer(scratch), ec);
		if (ec == asio::error::eof || ec == beast::http::error::end_of_stream) {
			return true;
		}
		if (ec) {
			return false;
		}
		if (bytes == 0) {
			return true;
		}
	}
}

} // namespace

server_fixture::server_fixture(warp::server::server_builder builder)
    : server_fixture(std::move(builder), event_loop_mode_tag<event_loop_mode::callbacks> {}) {
}

server_fixture::~server_fixture() {
	server.stop();
}

std::uint16_t server_fixture::reserve_port() {
	asio::io_context ioc;
	tcp::acceptor acceptor(ioc, tcp::endpoint {tcp::v4(), 0});
	return acceptor.local_endpoint().port();
}

std::string make_get_request(std::string_view path, std::string_view connection) {
	return "GET " + std::string(path) +
	       " HTTP/1.1\r\n"
	       "Host: 127.0.0.1\r\n"
	       "Connection: " +
	       std::string(connection) +
	       "\r\n"
	       "\r\n";
}

std::unique_ptr<client_connection> connect_client(std::uint16_t port) {
	auto client = std::make_unique<client_connection>();
	client->stream.expires_after(5s);

	const auto endpoint = tcp::endpoint {asio::ip::make_address("127.0.0.1"), port};
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	for (;;) {
		beast::error_code ec;
		client->stream.socket().close(ec);
		client->stream.connect(endpoint, ec);
		if (!ec) {
			break;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			throw std::runtime_error("timed out connecting to integration test server");
		}
		std::this_thread::sleep_for(20ms);
	}
	return client;
}

warp::ssl::ssl_config make_test_server_ssl_config() {
	return warp::ssl::ssl_config(
	    true, warp::ssl::file_cert_loader(test_source_path("tests/fixtures/tls/test_server_identity.pem")));
}

std::unique_ptr<tls_client_connection> connect_tls_client(std::uint16_t port) {
	auto client = std::make_unique<tls_client_connection>();
	client->stream.set_verify_mode(ssl::verify_peer);
	client->ssl_ctx.add_certificate_authority(asio::buffer(kTestCertificatePem, std::strlen(kTestCertificatePem)));
	beast::get_lowest_layer(client->stream).expires_after(5s);

	const auto endpoint = tcp::endpoint {asio::ip::make_address("127.0.0.1"), port};
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	for (;;) {
		beast::error_code ec;
		beast::get_lowest_layer(client->stream).socket().close(ec);
		beast::get_lowest_layer(client->stream).connect(endpoint, ec);
		if (ec) {
			if (std::chrono::steady_clock::now() >= deadline) {
				throw std::runtime_error("timed out connecting to integration test server");
			}
			std::this_thread::sleep_for(20ms);
			continue;
		}
		client->stream.handshake(ssl::stream_base::client, ec);
		if (ec) {
			throw std::runtime_error("tls handshake with trusted test certificate failed");
		}
		break;
	}

	return client;
}

boost::system::error_code connect_tls_client_without_trust(std::uint16_t port) {
	tls_client_connection client;
	client.stream.set_verify_mode(ssl::verify_peer);
	beast::get_lowest_layer(client.stream).expires_after(5s);

	const auto endpoint = tcp::endpoint {asio::ip::make_address("127.0.0.1"), port};
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	for (;;) {
		beast::error_code ec;
		beast::get_lowest_layer(client.stream).socket().close(ec);
		beast::get_lowest_layer(client.stream).connect(endpoint, ec);
		if (ec) {
			if (std::chrono::steady_clock::now() >= deadline) {
				throw std::runtime_error("timed out connecting to integration test server");
			}
			std::this_thread::sleep_for(20ms);
			continue;
		}
		client.stream.handshake(ssl::stream_base::client, ec);
		return ec;
	}

	return {};
}

void send_requests(client_connection &client, std::string_view payload) {
	client.stream.expires_after(5s);
	write_payload(client.stream, payload);
}

void send_requests(tls_client_connection &client, std::string_view payload) {
	beast::get_lowest_layer(client.stream).expires_after(5s);
	write_payload(client.stream, payload);
}

http_response read_response(client_connection &client) {
	client.stream.expires_after(5s);
	return read_http_response(client.stream, client.buffer);
}

http_response read_response(tls_client_connection &client) {
	beast::get_lowest_layer(client.stream).expires_after(5s);
	return read_http_response(client.stream, client.buffer);
}

bool next_response_is_eof(client_connection &client) {
	client.stream.expires_after(5s);
	return read_http_eof(client.stream, client.buffer);
}

bool next_response_is_eof(tls_client_connection &client) {
	beast::get_lowest_layer(client.stream).expires_after(5s);
	return read_http_eof(client.stream, client.buffer);
}

bool read_until_eof(client_connection &client) {
	client.stream.expires_after(5s);
	return drain_until_eof(client.stream);
}

bool read_until_eof(tls_client_connection &client) {
	beast::get_lowest_layer(client.stream).expires_after(5s);
	return drain_until_eof(client.stream);
}

boost::json::object parse_object_body(const http_response &response) {
	return boost::json::parse(response.body()).as_object();
}

asio::awaitable<response> delayed_ok_response(std::chrono::milliseconds delay, std::function<std::string()> body_fn) {
	const auto executor = co_await asio::this_coro::executor;
	asio::steady_timer timer(executor);
	timer.expires_after(delay);
	co_await timer.async_wait(asio::use_awaitable);
	co_return response::ok(body_fn());
}

const char *event_loop_mode_name(event_loop_mode mode) {
	switch (mode) {
	case event_loop_mode::callbacks:
		return "Callbacks";
	case event_loop_mode::coroutines:
		return "Coroutines";
	}
	return "Unknown";
}

} // namespace warp::tests::integration_support
