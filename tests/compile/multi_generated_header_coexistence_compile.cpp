#include "generated_multi_header_primary_api_resources.hpp"
#include "generated_multi_header_secondary_api_resources.hpp"

#include <type_traits>

namespace primary = generated_multi_header_primary_api;
namespace secondary = generated_multi_header_secondary_api;

namespace {

template <typename Request>
concept has_generated_request_traits = requires(const warp::http::request &req) {
	{
		warp::codegen::request_contract_traits<Request>::parse(req)
	} -> std::same_as<warp::codegen::parse_result<Request>>;
};

template <typename Response>
concept has_generated_response_traits = requires {
	{ warp::codegen::response_contract_traits<Response>::status_code } -> std::convertible_to<unsigned>;
	{ warp::codegen::response_contract_traits<Response>::has_body } -> std::convertible_to<bool>;
};

static_assert(has_generated_request_traits<primary::users_health_request>);
static_assert(has_generated_request_traits<secondary::users_health_request>);
static_assert(has_generated_response_traits<primary::users_health_response>);
static_assert(has_generated_response_traits<secondary::users_health_response>);

} // namespace
