//
// Created by Dhanush Nadella on 4/30/26.
//

#pragma once

#include <memory>
#include <mutex>

#include <boost/asio/ssl/context.hpp>

#include "warp/ssl/ssl_config.hpp"

namespace warp::server {

class ssl_context_provider {
public:
	explicit ssl_context_provider(warp::ssl::ssl_config ssl_config);

	[[nodiscard]] std::shared_ptr<boost::asio::ssl::context> current() const;
	std::shared_ptr<boost::asio::ssl::context> load_latest_ssl_context();

private:
	static std::shared_ptr<boost::asio::ssl::context> build_ctx(std::string_view pem_bundle);

	warp::ssl::ssl_config ssl_config_;
	mutable std::mutex refresh_mutex_;
	// libc++ in this toolchain does not provide std::atomic<std::shared_ptr<T>>,
	// so we publish the current context with the shared_ptr atomic load/store APIs.
	std::shared_ptr<boost::asio::ssl::context> current_context_;
};

} // namespace warp::server
