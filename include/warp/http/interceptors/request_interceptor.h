//
// Created by Dhanush Nadella on 4/26/26.
//

#pragma once
#include "warp/http/request.hpp"

namespace warp::http {

template <int Priority>
class request_interceptor {
public:
    virtual ~request_interceptor() = default;
    virtual void intercept_request(request& req) = 0;
};

}
