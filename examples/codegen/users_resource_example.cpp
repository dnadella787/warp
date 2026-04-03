#include "warp/http/server.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

// This illustrative example shows the consumer-side shape after generating
// `generated_api_types.hpp` and `generated_api_resources.hpp`.
// See `generate_users_headers.cpp` for the file-emission step.

namespace generated_api {

struct users_create_user_request_body {
	std::string name {};
	std::optional<std::string> nickname {};
};

struct users_create_user_request {
	std::string user_id {};
	std::optional<bool> verbose {};
	std::string x_trace_id {};
	users_create_user_request_body body {};
};

struct users_create_user_response_body {
	std::int64_t id {};
	bool active {};
};

struct users_create_user_response {
	static constexpr unsigned status_code = 201;
	users_create_user_response_body body {};
};

struct users_health_request {};

struct users_health_response {
	static constexpr unsigned status_code = 204;
};

template <typename Service>
class users_api_routes {
public:
	explicit users_api_routes(std::shared_ptr<Service> service);
	void register_routes(warp::http::server_builder &builder) const;
};

} // namespace generated_api

class users_resource {
public:
	generated_api::users_create_user_response create_user(generated_api::users_create_user_request request) {
		generated_api::users_create_user_response response;
		response.body.id = 42;
		response.body.active = !request.body.name.empty();
		return response;
	}

	warp::awaitable<generated_api::users_health_response> health(generated_api::users_health_request) {
		co_return generated_api::users_health_response {};
	}
};

int main() {
	auto service = std::make_shared<users_resource>();
	generated_api::users_api_routes<users_resource> routes(service);
	auto server = warp::http::server_builder().address("127.0.0.1").port(8080).register_resource(routes).build();
	server.run();
	return 0;
}
