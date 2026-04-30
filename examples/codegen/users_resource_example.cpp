#include <memory>
#include <string>

#include "generated_api_resources.hpp"
#include "generated_api_types.hpp"
#include "warp/server/server_builder.hpp"

#include "../helpers.cpp"

class users_resource {
public:
	explicit users_resource() = default;

	// don't worry the middleware will make this no alloc handler dispatch using move semantics
	generated_api::users_create_user_request_handler_result
	create_user(generated_api::users_create_user_request request) {
		if (request.body.name.empty()) {
			return warp::response::bad_request("name must not be empty");
		}
		generated_api::users_create_user_response response;
		response.body.id = 42;
		response.body.active = true;
		return response;
	}

	generated_api::users_health_request_handler_result health(generated_api::users_health_request request) {
		generated_api::users_health_response response;
		return response;
	}
};

int main() {
	auto service = std::make_shared<users_resource>();
	generated_api::users_api_routes routes(service);
	auto authz_interceptor = example::authz_interceptor {"Bob"};
	auto log_interceptor = example::response_log_interceptor {};

	auto server = warp::server::server_builder()
	                  .address("127.0.0.1")
	                  .port(8080)
	                  .register_resource(routes)
	                  .interceptor<1>(authz_interceptor)
	                  .interceptor<1>(log_interceptor)
	                  .build();
	server.run();
	return 0;
}
