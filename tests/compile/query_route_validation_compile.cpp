#include "generated_query_routing_api_resources.hpp"

#include <memory>
#include <type_traits>

#include "warp/http/server_builder.hpp"

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

template <typename Service>
concept generated_routes_registrable = requires(std::shared_ptr<Service> service, warp::http::server_builder &builder) {
	{ generated::reports_api_routes<Service> {service} };
	{ generated::reports_api_routes<Service> {service}.register_routes(builder) } -> std::same_as<void>;
};

static_assert(has_generated_request_traits<generated::reports_fetch_report_request>);
static_assert(has_generated_request_traits<generated::reports_fetch_report_summary_request>);
static_assert(has_generated_request_traits<generated::reports_fetch_report_projection_request>);
static_assert(has_generated_request_traits<generated::reports_fetch_report_summary_projection_request>);
static_assert(warp::http::deterministic_route_definitions<fallback_route, summary_route, projection_route,
                                                          summary_projection_route>());
static_assert(generated_routes_registrable<compile_time_reports_service>);
static_assert(warp::http::resource_registrable<generated::reports_api_routes<compile_time_reports_service> &>);

} // namespace
