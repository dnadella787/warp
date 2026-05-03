#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "warp/ssl/cert_loader.hpp"

namespace warp::ssl {

class ssl_config {
public:
	ssl_config() = default;

	template <pem_bundle_cert_loader Loader>
	ssl_config(bool enabled, Loader loader)
	    : enabled_(enabled), loader_(std::make_shared<std::remove_cvref_t<Loader>>(std::move(loader))) {
	}

	[[nodiscard]] bool enabled() const noexcept {
		return enabled_;
	}

	[[nodiscard]] std::string load_pem_bundle() const {
		if (!loader_) {
			throw std::logic_error("enabled TLS configuration requires a certificate loader");
		}
		return loader_->load_pem_bundle();
	}

private:
	bool enabled_ {false};
	std::shared_ptr<const cert_loader> loader_;
};

} // namespace warp::ssl
