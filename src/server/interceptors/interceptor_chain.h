//
// Created by Dhanush Nadella on 4/29/26.
//

#pragma once

#include <utility>
#include <vector>

#include "warp/server/router/interceptor.h"


namespace warp::server {

// note that interceptor_chain does not have any sense of priorities, list
// must be ordered before hand
class interceptor_chain {
public:
	interceptor_chain() = default;

	explicit interceptor_chain(std::vector<detail::type_erased_interceptor> interceptors)
	    : interceptors_(std::move(interceptors)) {}

	[[nodiscard]] bool empty() const noexcept {
		return interceptors_.empty();
	}

	[[nodiscard]] http::interceptor_result run(http::request &req) const {
		for (const auto &interceptor : interceptors_) {
			if (auto result = interceptor(req); result.has_value())
				return result;
		}
		return std::nullopt;
	}

private:
	std::vector<detail::type_erased_interceptor> interceptors_;
};

} // namespace warp::server
