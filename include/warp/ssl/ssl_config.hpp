#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "file_cert_loader.hpp"
#include "warp/server/job.hpp"
#include "warp/ssl/cert_loader.hpp"

namespace warp::ssl {

class ssl_config {
public:
	ssl_config() = default;

	template <pem_bundle_cert_loader Loader>
	ssl_config(bool enabled, Loader loader, job::job_config refresh_config = {})
	    : enabled_(enabled), loader_(std::make_shared<std::remove_cvref_t<Loader>>(std::move(loader))),
	      refresh_config_(refresh_config) {
	}

	[[nodiscard]] bool enabled() const noexcept {
		return enabled_;
	}

	[[nodiscard]] bool have_certs_changed() const {
		if (!loader_) {
			throw std::logic_error("enabled TLS configuration requires a certificate loader");
		}
		return loader_->have_certs_changed();
	}

	[[nodiscard]] std::string load_pem_bundle() const {
		if (!loader_) {
			throw std::logic_error("enabled TLS configuration requires a certificate loader");
		}
		return loader_->load_pem_bundle();
	}

	[[nodiscard]] const warp::job::job_config &refresh_config() const noexcept {
		return refresh_config_;
	}

private:
	bool enabled_ {false};
	std::shared_ptr<const cert_loader> loader_;
	job::job_config refresh_config_ {};
};

} // namespace warp::ssl
