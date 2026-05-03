#pragma once

#include <utility>

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
	explicit transport_provider(warp::ssl::ssl_config ssl_config) : ssl_context_provider_(std::move(ssl_config)) {
	}

	[[nodiscard]] tls_session_transport make_transport() const {
		return tls_session_transport(ssl_context_provider_.current());
	}

private:
	ssl_context_provider ssl_context_provider_;
};

} // namespace warp::server
