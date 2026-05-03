//
// Created by Dhanush Nadella on 4/30/26.
//

#include "ssl_context_provider.h"

#include <atomic>
#include <stdexcept>

#include <boost/asio/buffer.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace warp::server {

namespace {

void verify_private_key_matches_certificate_chain(boost::asio::ssl::context &native_context) {
	if (SSL_CTX_check_private_key(native_context.native_handle()) == 1) {
		return;
	}

	const unsigned long error_code = ERR_get_error();
	std::string message = "TLS private key does not match the configured certificate chain";
	if (const char *reason = ERR_reason_error_string(error_code)) {
		message += ": ";
		message += reason;
	}
	throw std::runtime_error(message);
}

} // namespace

ssl_context_provider::ssl_context_provider(ssl::ssl_config ssl_config) : ssl_config_(std::move(ssl_config)) {
	if (!ssl_config_.enabled()) {
		throw std::invalid_argument("ssl_context_provider requires enabled TLS configuration");
	}
	std::atomic_store_explicit(&current_context_, build_ctx(ssl_config_), std::memory_order_release);
}

std::shared_ptr<boost::asio::ssl::context> ssl_context_provider::current() const {
	return std::atomic_load_explicit(&current_context_, std::memory_order_acquire);
}

std::shared_ptr<boost::asio::ssl::context> ssl_context_provider::build_ctx(const warp::ssl::ssl_config &ssl_config) {
	auto native_context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);
	native_context->set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
	                            boost::asio::ssl::context::single_dh_use);

	const auto pem_bundle = ssl_config.load_pem_bundle();
	const auto pem_buffer = boost::asio::buffer(pem_bundle.data(), pem_bundle.size());
	native_context->use_certificate_chain(pem_buffer);
	native_context->use_private_key(pem_buffer, boost::asio::ssl::context::pem);
	verify_private_key_matches_certificate_chain(*native_context);
	return native_context;
}

} // namespace warp::server
