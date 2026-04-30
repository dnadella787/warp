//
// Created by Dhanush Nadella on 4/29/26.
//

#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "warp/server/interceptor/interceptor.h"


namespace warp::server {

template<typename T>
concept interceptor_type = std::is_same_v<T, http::response> || std::is_same_v<T, http::request>;

namespace detail {
	template <interceptor_type T>
	struct interceptor_chain_traits {
		static constexpr bool is_request = std::is_same_v<T, http::request>;

		using arg_type = T;
		using result_type = std::conditional_t<is_request, http::req_interceptor_result, void>;
		using interceptor_type = std::conditional_t<is_request, type_erased_req_interceptor, type_erased_resp_interceptor>;
	};


}

// note that interceptor_chain does not have any sense of priorities, list
// must be ordered beforehand
template<interceptor_type T>
class interceptor_chain {
public:
	using traits = detail::interceptor_chain_traits<T>;

	using stored_interceptor_type = traits::interceptor_type;
	using arg_type = traits::arg_type;
	using result_type = traits::result_type;

	interceptor_chain() = default;

	explicit interceptor_chain(std::vector<stored_interceptor_type> interceptors)
	    : interceptors_(std::move(interceptors)) {}

	[[nodiscard]] bool empty() const noexcept {
		return interceptors_.empty();
	}

	// TODO: Add support for req_ctx injection w/ response for resp interceptor Issue#33
	[[nodiscard]] result_type run(T &input) const {
		if constexpr (traits::is_request) {
			for (const auto &interceptor : interceptors_) {
				if (auto result = interceptor(input); result.has_value())
					return result;
			}
			return std::nullopt;
		} else {
			for (const auto &interceptor : interceptors_) {
				interceptor(input);
			}
			return;
		}
	}

private:
	std::vector<stored_interceptor_type> interceptors_;
};

} // namespace warp::server
