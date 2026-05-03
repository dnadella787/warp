#pragma once

#include <utility>

#include "server/job/ssl_refresh_job.h"
#include "transport.h"
#include "ssl/ssl_context_provider.h"

namespace warp::server {

template <warp_session_transport Transport>
class transport_provider;

template <>
class transport_provider<plain_session_transport> {
public:
	[[nodiscard]] plain_session_transport make_transport() const {
		return {};
	}
};

template <>
class transport_provider<tls_session_transport> {
public:
	explicit transport_provider(warp::ssl::ssl_config ssl_config)
	    : refresh_config_(ssl_config.refresh_config()),
	      ssl_context_provider_(std::make_shared<ssl_context_provider>(std::move(ssl_config))) {
	}

	[[nodiscard]] tls_session_transport make_transport() const {
		return tls_session_transport(ssl_context_provider_->current());
	}

	[[nodiscard]] ssl_refresh_job make_refresh_job() const {
		return ssl_refresh_job(ssl_context_provider_, refresh_config_);
	}

private:
	warp::job::job_config refresh_config_;
	std::shared_ptr<ssl_context_provider> ssl_context_provider_;
};

} // namespace warp::server
