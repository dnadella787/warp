#include "warp/http/server.hpp"

#include <memory>
#include <string>

#include "generated_api_resources.hpp"
#include "generated_api_types.hpp"

class users_resource {
public:
	explicit users_resource() = default;

	generated_api::users_create_user_response create_user(generated_api::users_create_user_request request) {
		generated_api::users_create_user_response response;
		response.body.id = 42;
		response.body.active = !request.body.name.empty();
		return response;
	}

	generated_api::users_health_response health(generated_api::users_health_request request) {
		generated_api::users_health_response response;
		return response;
	}
};

int main() {
	auto service = std::make_shared<users_resource>();
	generated_api::users_api_routes routes(service);
	auto server = warp::http::server_builder().address("127.0.0.1").port(8080).register_resource(routes).build();
	server.run();
	return 0;
}
