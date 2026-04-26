#include "generated_query_routing_api_resources.hpp"

#include <memory>
#include <type_traits>
#include <utility>

#include "warp/server/server_builder.hpp"

namespace generated = generated_query_routing_api;

namespace {

using summary_route =
    warp::http::route_spec<warp::method::get, "/reports/{report_id}", warp::http::required_query<"summary">,
                           warp::http::forbidden_query<"fields">>;
using projection_route =
    warp::http::route_spec<warp::method::get, "/reports/{report_id}", warp::http::forbidden_query<"summary">,
                           warp::http::required_query<"fields">>;
using summary_projection_route =
    warp::http::route_spec<warp::method::get, "/reports/{report_id}", warp::http::required_query<"summary">,
                           warp::http::required_query<"fields">>;
using fallback_route = warp::http::route_spec<warp::method::get, "/reports/{report_id}">;
using exact_mode_route =
    warp::http::route_spec<warp::method::get, "/items", warp::http::required_query_value<"mode", "full">>;
using broad_mode_route = warp::http::route_spec<warp::method::get, "/items", warp::http::required_query<"mode">>;
using optional_exact_mode_route =
    warp::http::route_spec<warp::method::get, "/items", warp::http::optional_query_value<"mode", "full">>;
using optional_broad_mode_route =
    warp::http::route_spec<warp::method::get, "/items", warp::http::optional_query<"mode">>;

class compile_time_reports_service {
public:
	generated::reports_fetch_report_response fetch_report(generated::reports_fetch_report_request request) {
		return generated::reports_fetch_report_response::builder()
		    .body(generated::reports_fetch_report_response_body::builder()
		              .route("full")
		              .report_id(request.report_id())
		              .saw_slow_started(false)
		              .build())
		    .build();
	}

	generated::reports_fetch_report_summary_response
	fetch_report_summary(generated::reports_fetch_report_summary_request request) {
		return generated::reports_fetch_report_summary_response::builder()
		    .body(generated::reports_fetch_report_summary_response_body::builder()
		              .route("summary")
		              .report_id(request.report_id())
		              .summary(request.summary())
		              .build())
		    .build();
	}

	generated::reports_fetch_report_projection_response
	fetch_report_projection(generated::reports_fetch_report_projection_request request) {
		return generated::reports_fetch_report_projection_response::builder()
		    .body(generated::reports_fetch_report_projection_response_body::builder()
		              .route("projection")
		              .report_id(request.report_id())
		              .fields(request.fields())
		              .build())
		    .build();
	}

	warp::awaitable<generated::reports_fetch_report_summary_projection_response>
	fetch_report_summary_projection(generated::reports_fetch_report_summary_projection_request request) {
		co_return generated::reports_fetch_report_summary_projection_response::builder()
		    .body(generated::reports_fetch_report_summary_projection_response_body::builder()
		              .route("summary_projection")
		              .report_id(request.report_id())
		              .summary(request.summary())
		              .fields(request.fields())
		              .fast_finished_before_return(false)
		              .build())
		    .build();
	}
};

template <typename Request>
concept has_generated_request_traits = requires(const warp::http::request &req) {
	{
		warp::codegen::request_contract_traits<Request>::parse(req)
	} -> std::same_as<warp::codegen::parse_result<Request>>;
};

template <typename Response>
concept has_generated_response_body_traits = requires(const Response &const_response, Response &&moved_response) {
	typename warp::codegen::response_contract_traits<Response>::response_type;
	{ warp::codegen::response_contract_traits<Response>::status_code } -> std::convertible_to<unsigned>;
	{ warp::codegen::response_contract_traits<Response>::has_body } -> std::convertible_to<bool>;
	requires warp::codegen::response_contract_traits<Response>::has_body;
	warp::codegen::response_contract_traits<Response>::body(const_response);
	warp::codegen::response_contract_traits<Response>::body(std::move(moved_response));
};

template <typename Service>
concept generated_routes_registrable =
    requires(std::shared_ptr<Service> service, warp::server::server_builder &builder) {
	    { generated::reports_api_routes<Service> {service} };
	    { generated::reports_api_routes<Service> {service}.register_routes(builder) } -> std::same_as<void>;
    };

static_assert(has_generated_request_traits<generated::reports_fetch_report_request>);
static_assert(has_generated_request_traits<generated::reports_fetch_report_summary_request>);
static_assert(has_generated_request_traits<generated::reports_fetch_report_projection_request>);
static_assert(has_generated_request_traits<generated::reports_fetch_report_summary_projection_request>);
static_assert(has_generated_response_body_traits<generated::reports_fetch_report_response>);
static_assert(has_generated_response_body_traits<generated::reports_fetch_report_summary_response>);
static_assert(
    std::same_as<decltype(warp::codegen::response_contract_traits<generated::reports_fetch_report_response>::body(
                     std::declval<const generated::reports_fetch_report_response &>())),
                 const generated::reports_fetch_report_response_body &>);
static_assert(
    std::same_as<decltype(warp::codegen::response_contract_traits<generated::reports_fetch_report_response>::body(
                     std::declval<generated::reports_fetch_report_response &&>())),
                 generated::reports_fetch_report_response_body &&>);
static_assert(std::same_as<
              decltype(warp::codegen::to_http_response(std::declval<generated::reports_fetch_report_response>(), 11)),
              warp::response>);
static_assert(warp::http::deterministic_route_definitions<fallback_route, summary_route, projection_route,
                                                          summary_projection_route>());
static_assert(warp::http::deterministic_route_definitions<exact_mode_route, broad_mode_route>());
static_assert(!warp::http::deterministic_route_definitions<optional_exact_mode_route, optional_broad_mode_route>());
static_assert(warp::server::detail::percent_encode_query_component("plus+space %") == "plus%2Bspace%20%25");
static_assert(warp::server::detail::registered_query_constraint_fragment(warp::http::query_constraint_descriptor {
                  .name = "plus+space %",
                  .presence = warp::http::query_constraint_presence::optional,
                  .has_exact_value = true,
                  .exact_value = "a+b&c=d%",
              }) == "~plus%2Bspace%20%25=a%2Bb%26c%3Dd%25");
static_assert(generated_routes_registrable<compile_time_reports_service>);
static_assert(warp::server::resource_registrable<generated::reports_api_routes<compile_time_reports_service> &>);

} // namespace
