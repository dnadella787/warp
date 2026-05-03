//
// Created by Dhanush Nadella on 5/3/26.
//

#pragma once

#include <memory>

#include "ssl/ssl_context_provider.h"
#include "warp/server/job.hpp"

namespace warp::server {

class ssl_refresh_job {
public:
	ssl_refresh_job() = delete;

	explicit ssl_refresh_job(std::shared_ptr<ssl_context_provider> provider, warp::job::job_config config = {})
	    : provider_(std::move(provider)), config_(config) {
	}

	[[nodiscard]] bool run() const {
		try {
			provider_->load_latest_ssl_context();
			return true;
		} catch (...) {
			return false;
		}
	}

	[[nodiscard]] warp::job::job_config config() const {
		return config_;
	}

private:
	std::shared_ptr<ssl_context_provider> provider_;
	warp::job::job_config config_;
};

} // namespace warp::server
