#include "generated_overloaded_handler_api_resources.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "warp/http/server_builder.hpp"

namespace generated = generated_overloaded_handler_api;

namespace {

class overloaded_codegen_service {
public:
	generated::users_health_response health(generated::users_health_request request) {
		last_user_id_ = std::move(request).user_id();
		return {};
	}

	generated::users_health_response health(generated::admin_health_request) {
		return {};
	}

	warp::awaitable<generated::admin_health_response> health(generated::admin_health_request) const {
		co_return generated::admin_health_response {};
	}

private:
	mutable std::string last_user_id_;
};

class request_distinguished_service {
public:
	generated::users_health_response health(generated::users_health_request request) {
		last_user_id_ = std::move(request).user_id();
		return {};
	}

	warp::awaitable<generated::admin_health_response> health(generated::admin_health_request) const {
		co_return generated::admin_health_response {};
	}

private:
	mutable std::string last_user_id_;
};

class same_request_cv_selected_service {
public:
	generated::admin_health_response health(generated::users_health_request request) {
		last_user_id_ = std::move(request).user_id();
		return {};
	}

	generated::users_health_response health(const generated::users_health_request &request) const {
		last_const_user_id_ = request.user_id();
		return {};
	}

	warp::awaitable<generated::admin_health_response> health(generated::admin_health_request) const {
		co_return generated::admin_health_response {};
	}

private:
	mutable std::string last_user_id_;
	mutable std::string last_const_user_id_;
};

template <typename Service>
concept generated_routes_registrable = requires(std::shared_ptr<Service> service, warp::http::server_builder &builder) {
	{ generated::users_api_routes<Service> {service} };
	{ generated::admin_api_routes<Service> {service} };
	{ generated::users_api_routes<Service> {service}.register_routes(builder) } -> std::same_as<void>;
	{ generated::admin_api_routes<Service> {service}.register_routes(builder) } -> std::same_as<void>;
};

template <typename Service>
concept generated_resources_registrable =
    requires(warp::http::server_builder &builder, generated::users_api_routes<Service> &users,
             generated::admin_api_routes<Service> &admin) {
	    { warp::codegen::register_resources(builder, users, admin) } -> std::same_as<void>;
    };

static_assert(generated_routes_registrable<overloaded_codegen_service>);
static_assert(generated_resources_registrable<overloaded_codegen_service>);
static_assert(warp::http::resource_registrable<generated::users_api_routes<overloaded_codegen_service> &>);
static_assert(warp::http::resource_registrable<generated::admin_api_routes<overloaded_codegen_service> &>);
static_assert(generated_routes_registrable<request_distinguished_service>);
static_assert(generated_routes_registrable<same_request_cv_selected_service>);

} // namespace
