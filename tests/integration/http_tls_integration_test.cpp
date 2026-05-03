#include "support/integration/http_integration_harness.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/address.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "warp/server/server_builder.hpp"
#include "warp/ssl/file_cert_loader.hpp"

namespace warp::tests {

namespace http = boost::beast::http;
namespace support = integration_support;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

std::string test_source_path(std::string_view relative_path) {
	return std::string(WARP_TEST_SOURCE_DIR) + "/" + std::string(relative_path);
}

std::string tls_fixture_path(std::string_view filename) {
	return std::string(WARP_TEST_TLS_FIXTURE_DIR) + "/" + std::string(filename);
}

std::string read_file_exact(std::string_view path) {
	std::ifstream input(std::string(path), std::ios::binary);
	if (!input) {
		throw std::runtime_error("failed to open TLS test fixture");
	}
	return std::string(std::istreambuf_iterator(input), std::istreambuf_iterator<char>());
}

struct temp_dir_guard {
	fs::path path;

	~temp_dir_guard() {
		std::error_code ec;
		fs::remove_all(path, ec);
	}
};

void replace_file_atomically(const fs::path &source, const fs::path &target) {
	const auto temp_path = target.parent_path() / (target.filename().string() + ".tmp");
	fs::copy_file(source, temp_path, fs::copy_options::overwrite_existing);
	fs::rename(temp_path, target);
}

std::string sha256_fingerprint(X509 *certificate) {
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
	unsigned int digest_length = 0;
	if (X509_digest(certificate, EVP_sha256(), digest.data(), &digest_length) != 1) {
		throw std::runtime_error("failed to compute X509 fingerprint");
	}

	std::ostringstream output;
	output << std::uppercase << std::hex << std::setfill('0');
	for (unsigned int i = 0; i < digest_length; ++i) {
		if (i != 0) {
			output << ':';
		}
		output << std::setw(2) << static_cast<unsigned int>(digest[i]);
	}
	return output.str();
}

std::string certificate_fingerprint_from_pem_file(const fs::path &pem_path) {
	auto file = std::unique_ptr<FILE, decltype(&std::fclose)>(std::fopen(pem_path.c_str(), "rb"), &std::fclose);
	if (!file) {
		throw std::runtime_error("failed to open PEM bundle fixture");
	}

	auto certificate =
	    std::unique_ptr<X509, decltype(&X509_free)>(PEM_read_X509(file.get(), nullptr, nullptr, nullptr), &X509_free);
	if (!certificate) {
		throw std::runtime_error("failed to parse certificate from PEM bundle fixture");
	}

	return sha256_fingerprint(certificate.get());
}

std::unique_ptr<support::tls_client_connection> connect_tls_client_with_ca(std::uint16_t port,
                                                                           std::string_view ca_pem) {
	auto client = std::make_unique<support::tls_client_connection>();
	client->stream.set_verify_mode(support::ssl::verify_peer);
	client->ssl_ctx.add_certificate_authority(support::asio::buffer(ca_pem.data(), ca_pem.size()));
	support::beast::get_lowest_layer(client->stream).expires_after(5s);

	const auto endpoint = support::tcp::endpoint {support::asio::ip::make_address("127.0.0.1"), port};
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	for (;;) {
		support::beast::error_code ec;
		support::beast::get_lowest_layer(client->stream).socket().close(ec);
		support::beast::get_lowest_layer(client->stream).connect(endpoint, ec);
		if (ec) {
			if (std::chrono::steady_clock::now() >= deadline) {
				throw std::runtime_error("timed out connecting trusted TLS client to integration test server");
			}
			std::this_thread::sleep_for(20ms);
			continue;
		}

		client->stream.handshake(support::ssl::stream_base::client, ec);
		if (ec) {
			throw std::runtime_error("trusted TLS client handshake failed");
		}
		return client;
	}
}

std::string peer_certificate_fingerprint(support::tls_client_connection &client) {
	auto certificate = std::unique_ptr<X509, decltype(&X509_free)>(
	    SSL_get1_peer_certificate(client.stream.native_handle()), &X509_free);
	if (!certificate) {
		throw std::runtime_error("TLS peer certificate missing");
	}
	return sha256_fingerprint(certificate.get());
}

} // namespace

template <typename ModeTag>
class HttpTlsIntegrationTest : public ::testing::Test {};

using EventLoopModes = ::testing::Types<support::event_loop_mode_tag<event_loop_mode::callbacks>,
                                        support::event_loop_mode_tag<event_loop_mode::coroutines>>;

struct EventLoopModeNames {
	template <typename ModeTag>
	static std::string GetName(int) {
		return support::event_loop_mode_name(ModeTag::value);
	}
};

TYPED_TEST_SUITE(HttpTlsIntegrationTest, EventLoopModes, EventLoopModeNames);

TYPED_TEST(HttpTlsIntegrationTest, TrustedTlsClientGetsResponseAndGracefulClose) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	auto client = support::connect_tls_client(fixture.port);
	support::send_requests(*client, support::make_get_request("/secure", "close"));

	const auto resp = support::read_response(*client);
	EXPECT_EQ(resp.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(resp).at("route").as_string()), "secure");
	EXPECT_TRUE(support::read_until_eof(*client));
}

TYPED_TEST(HttpTlsIntegrationTest, StalledTlsClientAfterFinalResponseTimesOutAndServerClosesConnection) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	auto stalled_client = support::connect_tls_client(fixture.port);
	support::send_requests(*stalled_client, support::make_get_request("/secure", "close"));

	const auto stalled_response = support::read_response(*stalled_client);
	EXPECT_EQ(stalled_response.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(stalled_response).at("route").as_string()), "secure");

	EXPECT_TRUE(support::read_until_eof(*stalled_client));
}

TYPED_TEST(HttpTlsIntegrationTest, PlainHttpClientToTlsServerIsRejectedAndServerRecovers) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	auto plain_client = support::connect_client(fixture.port);
	support::send_requests(*plain_client, support::make_get_request("/secure", "close"));
	EXPECT_TRUE(support::next_response_is_eof(*plain_client));

	auto tls_client = support::connect_tls_client(fixture.port);
	support::send_requests(*tls_client, support::make_get_request("/secure", "close"));
	const auto resp = support::read_response(*tls_client);
	EXPECT_EQ(resp.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(resp).at("route").as_string()), "secure");
	EXPECT_TRUE(support::read_until_eof(*tls_client));
}

TYPED_TEST(HttpTlsIntegrationTest, UntrustedTlsClientIsRejectedWithoutBreakingLaterTrustedClients) {
	support::server_fixture fixture(warp::server::server_builder()
	                                    .ssl_config(support::make_test_server_ssl_config())
	                                    .get<"/secure">([](const request &) -> response {
		                                    return response::ok(body_builder().set("route", "secure").build());
	                                    }),
	                                TypeParam {});

	const auto handshake_error = support::connect_tls_client_without_trust(fixture.port);
	EXPECT_TRUE(static_cast<bool>(handshake_error));

	auto trusted_client = support::connect_tls_client(fixture.port);
	support::send_requests(*trusted_client, support::make_get_request("/secure", "close"));
	const auto resp = support::read_response(*trusted_client);
	EXPECT_EQ(resp.result(), http::status::ok);
	EXPECT_EQ(std::string(support::parse_object_body(resp).at("route").as_string()), "secure");
	EXPECT_TRUE(support::read_until_eof(*trusted_client));
}

TYPED_TEST(HttpTlsIntegrationTest, TlsRefreshJobPublishesRotatedCertificateForNewConnections) {
	const auto ca_path = fs::path(tls_fixture_path("rotation_ca.pem"));
	const auto source_a_path = fs::path(tls_fixture_path("rotation_source_a.bundle.pem"));
	const auto source_b_path = fs::path(tls_fixture_path("rotation_source_b.bundle.pem"));
	const auto temp_dir =
	    fs::temp_directory_path() /
	    ("warp-tls-refresh-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	fs::create_directories(temp_dir);
	temp_dir_guard cleanup {.path = temp_dir};

	const auto active_bundle_path = temp_dir / "active.bundle.pem";
	replace_file_atomically(source_a_path, active_bundle_path);

	support::server_fixture fixture(
	    warp::server::server_builder()
	        .ssl_config(warp::ssl::ssl_config(
	            true, warp::ssl::file_cert_loader(active_bundle_path.string()),
	            warp::job::job_config {.initial_delay_seconds = 0s, .interval = 1s, .max_retries = 1, .max_ttl = 30}))
	        .get<"/secure">([](const request &) -> response {
		        return response::ok(body_builder().set("route", "secure").build());
	        }),
	    TypeParam {});

	const auto ca_pem = read_file_exact(ca_path.string());
	const auto expected_a_fingerprint = certificate_fingerprint_from_pem_file(source_a_path);
	const auto expected_b_fingerprint = certificate_fingerprint_from_pem_file(source_b_path);

	auto initial_client = connect_tls_client_with_ca(fixture.port, ca_pem);
	EXPECT_EQ(peer_certificate_fingerprint(*initial_client), expected_a_fingerprint);
	support::send_requests(*initial_client, support::make_get_request("/secure", "close"));
	EXPECT_EQ(support::read_response(*initial_client).result(), http::status::ok);
	EXPECT_TRUE(support::read_until_eof(*initial_client));

	replace_file_atomically(source_b_path, active_bundle_path);
	const auto original_write_time = fs::last_write_time(active_bundle_path);
	fs::last_write_time(active_bundle_path, original_write_time + std::chrono::seconds(2));

	const auto deadline = std::chrono::steady_clock::now() + 8s;
	for (;;) {
		auto rotated_client = connect_tls_client_with_ca(fixture.port, ca_pem);
		const auto rotated_fingerprint = peer_certificate_fingerprint(*rotated_client);
		support::send_requests(*rotated_client, support::make_get_request("/secure", "close"));
		EXPECT_EQ(support::read_response(*rotated_client).result(), http::status::ok);
		EXPECT_TRUE(support::read_until_eof(*rotated_client));

		if (rotated_fingerprint == expected_b_fingerprint) {
			break;
		}

		if (std::chrono::steady_clock::now() >= deadline) {
			ADD_FAILURE() << "timed out waiting for refreshed TLS certificate";
			break;
		}
		std::this_thread::sleep_for(250ms);
	}
}

} // namespace warp::tests
