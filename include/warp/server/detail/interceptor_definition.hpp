#pragma once

#include "warp/server/interceptor/interceptor.h"

namespace warp::server::detail {

template <erased_interceptor_type ErasedInterceptor>
struct interceptor_definition {
	int priority;
	ErasedInterceptor callback;
};

} // namespace warp::server::detail
