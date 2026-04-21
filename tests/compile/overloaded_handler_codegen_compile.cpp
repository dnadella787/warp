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

class noexcept_codegen_service {
public:
	generated::users_health_response health(generated::users_health_request request) noexcept {
		last_user_id_ = std::move(request).user_id();
		return {};
	}

	warp::awaitable<generated::admin_health_response> health(generated::admin_health_request) const noexcept {
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

class sync_only_codegen_service {
public:
	generated::users_health_response health(generated::users_health_request request) noexcept {
		last_user_id_ = std::move(request).user_id();
		return {};
	}

	generated::admin_health_response health(generated::admin_health_request) const noexcept {
		return {};
	}

private:
	mutable std::string last_user_id_;
};

class mixed_result_codegen_service {
public:
	generated::users_health_request_handler_result health(generated::users_health_request request) {
		last_user_id_ = std::move(request).user_id();
		if (last_user_id_.empty()) {
			return warp::response::not_found("user health is unavailable");
		}
		return generated::users_health_response {};
	}

	warp::awaitable<generated::admin_health_request_handler_result> health(generated::admin_health_request) const {
		co_return warp::response::server_error("admin health failed");
	}

private:
	mutable std::string last_user_id_;
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

template <typename Service>
auto bind_users_health_handler(std::shared_ptr<Service> service) {
	return warp::codegen::bind_generated_endpoint<
	    warp::codegen::request_contract_traits<generated::users_health_request>,
	    warp::codegen::response_contract_traits<generated::users_health_response>, Service,
	    generated::codegen_detail::users_health_request_handler_selector>(std::move(service));
}

template <typename Service>
auto bind_admin_health_handler(std::shared_ptr<Service> service) {
	return warp::codegen::bind_generated_endpoint<
	    warp::codegen::request_contract_traits<generated::admin_health_request>,
	    warp::codegen::response_contract_traits<generated::admin_health_response>, Service,
	    generated::codegen_detail::admin_health_request_handler_selector>(std::move(service));
}

static_assert(generated_routes_registrable<overloaded_codegen_service>);
static_assert(generated_resources_registrable<overloaded_codegen_service>);
static_assert(warp::http::resource_registrable<generated::users_api_routes<overloaded_codegen_service> &>);
static_assert(warp::http::resource_registrable<generated::admin_api_routes<overloaded_codegen_service> &>);
static_assert(generated_routes_registrable<request_distinguished_service>);
static_assert(generated_routes_registrable<same_request_cv_selected_service>);
static_assert(generated_routes_registrable<noexcept_codegen_service>);
static_assert(generated_routes_registrable<sync_only_codegen_service>);
static_assert(generated_routes_registrable<mixed_result_codegen_service>);
static_assert(warp::http::is_sync_route_handler<
              decltype(bind_users_health_handler(std::declval<std::shared_ptr<sync_only_codegen_service>>()))>);
static_assert(!warp::http::is_async_route_handler<
              decltype(bind_users_health_handler(std::declval<std::shared_ptr<sync_only_codegen_service>>()))>);
static_assert(warp::http::is_sync_route_handler<
              decltype(bind_admin_health_handler(std::declval<std::shared_ptr<sync_only_codegen_service>>()))>);
static_assert(!warp::http::is_async_route_handler<
              decltype(bind_admin_health_handler(std::declval<std::shared_ptr<sync_only_codegen_service>>()))>);
static_assert(warp::http::is_sync_route_handler<
              decltype(bind_users_health_handler(std::declval<std::shared_ptr<mixed_result_codegen_service>>()))>);
static_assert(warp::http::is_async_route_handler<
              decltype(bind_admin_health_handler(std::declval<std::shared_ptr<mixed_result_codegen_service>>()))>);

} // namespace
