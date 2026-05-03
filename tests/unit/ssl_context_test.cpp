#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include "warp/ssl/cert_loader.hpp"
#include "ssl/ssl_context_provider.h"
#include "warp/ssl/file_cert_loader.hpp"
#include "warp/ssl/ssl_config.hpp"

namespace warp::tests {

namespace {
namespace fs = std::filesystem;

std::string test_source_path(std::string_view relative_path) {
	return std::string(WARP_TEST_SOURCE_DIR) + "/" + std::string(relative_path);
}

std::string read_file_exact(std::string_view path) {
	std::ifstream input(std::string(path), std::ios::binary);
	if (!input) {
		throw std::runtime_error("failed to open test fixture");
	}
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string certificate_only_pem_bundle(std::string_view path) {
	const std::string pem_bundle = read_file_exact(path);
	const std::string_view marker = "-----END CERTIFICATE-----";
	const std::size_t marker_pos = pem_bundle.find(marker);
	if (marker_pos == std::string::npos) {
		throw std::runtime_error("certificate marker not found in fixture");
	}

	const std::size_t newline_pos = pem_bundle.find('\n', marker_pos + marker.size());
	if (newline_pos == std::string::npos) {
		return pem_bundle.substr(0, marker_pos + marker.size());
	}
	return pem_bundle.substr(0, newline_pos + 1);
}

class mismatched_key_loader final : public warp::ssl::cert_loader {
public:
	explicit mismatched_key_loader(std::string fixture_path) : fixture_path_(std::move(fixture_path)) {
	}

	[[nodiscard]] bool have_certs_changed() const override {
		return true;
	}

	[[nodiscard]] std::string load_pem_bundle() const override {
		return certificate_only_pem_bundle(fixture_path_) + std::string(kMismatchedPrivateKeyPem);
	}

private:
	static constexpr std::string_view kMismatchedPrivateKeyPem = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC0sP+QhYa1OSck
OF3tZ1FAskYWFJUG0wIO1EZ+ewWoLM6NQTRQR+D3eYiTaibshyGXSM08RoTIq2DL
opXjXNePowdZG/5pajmIjy7GRVaGu8W5CgQheTYlckgxnwKJqLE5bGwE49K2E7pd
cuNn6wBnutJh3uMNLzQwt4nybCkUIqG9o1CbLsFtvq47xyNygv4h2lDjr13dxXHG
3uXMrHNMR1iJgRiKocdWFwec5rFkH8RJ7w8Wyzd6lgAhA2NOHeohAIrHmQOq+ePG
+tukAL8z1ULrPt75Ywav40I9h1AYofJ4HYKa2FqAxTaqdmLOV+e2AhvPuW/MXrKk
AVXOUjmVAgMBAAECggEAE4wNTaNgSAUTiqvNX3QcmmDey5804vzbPWLx8FdbLI8S
Vj4eTGwGNodvgrEKXm6c7+qIh1OCpFnD2iuvxt2hlTCYCCgK3PkImdERiJHyMxen
o2WlvkBIXwMus62FvwIgj/t+cQX7YsaeE+Nsne6Sh+YyQbO0wNWhMf78/Zx9Uifh
QZ3mNtvXWlIboyODv1QiWz4PLn2R44wyLIlpJKuMy4yBwfzQNXdSDgR28r9i5Gld
rnrXDNH+GerqRM7uQC/EZ79uQoseHGNRurKTNXwTwbprLXrR3Cja2wfXpSt6aPEh
ZW0DkRd5Mf5d3PcgbVkEwjUxSRdmI81iOGcGKGpN6QKBgQDplY/r3i9i2NPwWsxZ
OLtqUYJm8SkdR/cOHcW5ZuXrlBe4dTD6Tr8n+ujBsZzwxu+CWXdeilyNVUQclG8i
cdFNvGR66LcRYfUUD0E8avL07IK+Yn3jOTn2FzuYuW+F8UnH/asIDPrXwM5vGcQj
+Whhr7tAW3TTdVNogq7jy+NzrQKBgQDGCAYcl+BDbmrv884Csitkf4bUnU7yjz/c
dkgZUgIXop5OA5eYxFaMR1j0XpahnPi4KfzPu0EgK2dQAHrnMKFDjn4Jm1P9TFDz
bafXiCg9at+yOV6Vp5lRBJgTNHzDnKZ7ZTWpzqtkDxD9ja58KbYUhLZcRRg77FMe
Fecn6HjaiQKBgH7AjR+2Ksqd1KxJ6TfFRFYWMwf/d4sPIS5E06WfA8cJTrHmzhQW
JT7htepdokc5/IAkYlUoCb8b9OD5XzE2yBhB0disbaL+IAqpmIHbm0lzCiObuKpT
xHMY+lsOzUjGvX3L1kKBIKFxW9QIDFplHdJclOUAe+2/bep5d8PfQOblAoGAGam+
iQQRlwt/Jjt1LhpCz1JLedAtA/gWcY6Oh2F+TevQEhIbGjwPbzxrxbdgU+9QuCUQ
0ybUKMQXLmHxi3Zc37FemgYcG05Bi1phjufhNxxbgvA2VrSShNJQluSNapgpZwJV
svzKbzwYmpM60nJhW0VbkJePrWxR1StHen+A8ekCgYBIVEnZOuc5cb/THV3HrlHL
1grXW1dGWLGl71qAK9WdozT1RP/kk/uxwj0oXibteKJeWqFfbblFrLpud6LaOJhR
KcTyr1AG/Z0jceOL/ofZJXqOK1FCAy35lj1+wBMoZuu5sHWOO05QZ/Prd/Gve8OH
Fwvrj8ZFxyTXADuVG542mA==
-----END PRIVATE KEY-----
)";

	std::string fixture_path_;
};

class mutable_pem_loader final : public warp::ssl::cert_loader {
public:
	explicit mutable_pem_loader(std::shared_ptr<std::string> pem_bundle) : pem_bundle_(std::move(pem_bundle)) {
	}

	[[nodiscard]] bool have_certs_changed() const override {
		if (!has_loaded_) {
			return true;
		}
		return *pem_bundle_ != last_loaded_pem_bundle_;
	}

	[[nodiscard]] std::string load_pem_bundle() const override {
		last_loaded_pem_bundle_ = *pem_bundle_;
		has_loaded_ = true;
		return last_loaded_pem_bundle_;
	}

private:
	std::shared_ptr<std::string> pem_bundle_;
	mutable std::string last_loaded_pem_bundle_;
	mutable bool has_loaded_ {false};
};

struct temp_dir_guard {
	fs::path path;

	~temp_dir_guard() {
		std::error_code ec;
		fs::remove_all(path, ec);
	}
};

} // namespace

TEST(SslContextTest, FileCertLoaderReturnsExactPemBundleBytes) {
	const auto fixture_path = test_source_path("tests/fixtures/tls/test_server_identity.pem");

	warp::ssl::file_cert_loader loader(fixture_path);

	EXPECT_EQ(loader.load_pem_bundle(), read_file_exact(fixture_path));
}

TEST(SslContextTest, ProviderBuildsNativeTlsContextFromPemBundle) {
	warp::server::ssl_context_provider provider(warp::ssl::ssl_config(
	    true, warp::ssl::file_cert_loader(test_source_path("tests/fixtures/tls/test_server_identity.pem"))));

	const auto native_context = provider.current();

	ASSERT_NE(native_context, nullptr);
	EXPECT_TRUE(native_context->native_handle() != nullptr);
}

TEST(SslContextTest, ProviderRejectsMismatchedPrivateKeyAndCertificateChain) {
	const auto fixture_path = test_source_path("tests/fixtures/tls/test_server_identity.pem");

	EXPECT_THROW(warp::server::ssl_context_provider(warp::ssl::ssl_config(true, mismatched_key_loader(fixture_path))),
	             std::runtime_error);
}

TEST(SslContextTest, ProviderLoadsLatestContextOnlyWhenPemBundleChanges) {
	auto pem_bundle =
	    std::make_shared<std::string>(read_file_exact(test_source_path("tests/fixtures/tls/test_server_identity.pem")));
	warp::server::ssl_context_provider provider(warp::ssl::ssl_config(true, mutable_pem_loader(pem_bundle)));

	const auto initial_context = provider.current();
	ASSERT_NE(initial_context, nullptr);

	EXPECT_EQ(provider.load_latest_ssl_context(), initial_context);
	EXPECT_EQ(provider.current(), initial_context);

	pem_bundle->append("\n");
	const auto refreshed_context = provider.load_latest_ssl_context();

	ASSERT_NE(refreshed_context, nullptr);
	EXPECT_NE(refreshed_context, initial_context);
	EXPECT_EQ(provider.current(), refreshed_context);
}

TEST(SslContextTest, FileCertLoaderDetectsPemBundleChangesByLastWriteTime) {
	const auto fixture_pem = read_file_exact(test_source_path("tests/fixtures/tls/test_server_identity.pem"));
	const auto temp_dir = fs::temp_directory_path() /
	                      ("warp-file-cert-loader-" +
	                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-mtime");
	fs::create_directories(temp_dir);
	temp_dir_guard cleanup {.path = temp_dir};

	const auto pem_path = temp_dir / "bundle.pem";
	{
		std::ofstream output(pem_path, std::ios::binary);
		ASSERT_TRUE(output);
		output << fixture_pem;
	}

	warp::ssl::file_cert_loader loader(pem_path.string());

	EXPECT_EQ(loader.load_pem_bundle(), fixture_pem);
	EXPECT_FALSE(loader.have_certs_changed());

	const auto original_write_time = fs::last_write_time(pem_path);
	{
		std::ofstream output(pem_path, std::ios::binary | std::ios::trunc);
		ASSERT_TRUE(output);
		output << fixture_pem;
	}
	fs::last_write_time(pem_path, original_write_time + std::chrono::seconds(2));

	EXPECT_TRUE(loader.have_certs_changed());
	EXPECT_FALSE(loader.have_certs_changed());
}

TEST(SslContextTest, DisabledConfigDoesNotConstructProvider) {
	const warp::ssl::ssl_config disabled_config;

	EXPECT_FALSE(disabled_config.enabled());
}

} // namespace warp::tests
