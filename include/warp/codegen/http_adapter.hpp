#pragma once

#include "warp/codegen/detail/type_traits.hpp"
#include "warp/http/response_builder.hpp"
#include "warp/http/server.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/value_from.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "warp/warp.hpp"
#include "warp/http/server_builder.hpp"

#if defined(__APPLE__)
#include "fast_float/fast_float.h"
#define PARSE_FLOAT(from, to, out)          fast_float::from_chars(from, to, out)
#define PARSE_FLOAT_FMT(from, to, out, fmt) fast_float::from_chars(from, to, out)
#else
#include <charconv>
#define PARSE_FLOAT(from, to, out)          std::from_chars(from, to, out)
#define PARSE_FLOAT_FMT(from, to, out, fmt) std::from_chars(from, to, out, fmt)
#endif

namespace warp::codegen {

struct binding_error {
	boost::beast::http::status status {boost::beast::http::status::bad_request};
	std::string code;
	std::string message;
};

inline binding_error make_bad_request(std::string code, std::string message) {
	return binding_error {
	    .status = boost::beast::http::status::bad_request, .code = std::move(code), .message = std::move(message)};
}

inline binding_error make_unsupported_media_type(std::string code, std::string message) {
	return binding_error {.status = boost::beast::http::status::unsupported_media_type,
	                      .code = std::move(code),
	                      .message = std::move(message)};
}

template <typename T>
class parse_result {
public:
	static parse_result success(T value) {
		return parse_result(std::move(value));
	}

	static parse_result failure(binding_error error) {
		return parse_result(std::move(error));
	}

	[[nodiscard]] bool has_value() const noexcept {
		return std::holds_alternative<T>(storage_);
	}

	[[nodiscard]] T &value() & {
		return std::get<T>(storage_);
	}

	[[nodiscard]] const T &value() const & {
		return std::get<T>(storage_);
	}

	[[nodiscard]] T &&value() && {
		return std::move(std::get<T>(storage_));
	}

	[[nodiscard]] const binding_error &error() const {
		return std::get<binding_error>(storage_);
	}

private:
	explicit parse_result(T value) : storage_(std::move(value)) {
	}

	explicit parse_result(binding_error error) : storage_(std::move(error)) {
	}

	std::variant<T, binding_error> storage_;
};

inline response to_error_response(const binding_error &error, unsigned version = 11) {
	auto body = body_builder().set("error", error.message);
	if (!error.code.empty()) {
		body.set("code", error.code);
	}
	return response_builder().status(error.status).version(version).body(body.build()).build();
}

inline response to_bad_request_response(const binding_error &error, unsigned version = 11) {
	return to_error_response(error, version);
}

template <typename T>
struct request_contract_traits;

template <typename T>
struct response_contract_traits;

namespace detail {

template <typename T>
constexpr std::string_view scalar_type_name() {
	if constexpr (std::is_same_v<T, std::string>) {
		return "string";
	} else if constexpr (std::is_same_v<T, std::int64_t>) {
		return "int64";
	} else if constexpr (std::is_same_v<T, std::uint64_t>) {
		return "uint64";
	} else if constexpr (std::is_same_v<T, int>) {
		return "int";
	} else if constexpr (std::is_same_v<T, bool>) {
		return "bool";
	} else if constexpr (std::is_same_v<T, double>) {
		return "double";
	} else {
		return "value";
	}
}

template <typename T>
parse_result<T> parse_scalar_impl(std::string_view value, std::string_view field_name, std::string_view location_name) {
	if constexpr (std::is_same_v<T, std::string>) {
		return parse_result<T>::success(std::string(value));
	} else if constexpr (std::is_same_v<T, bool>) {
		if (value == "true" || value == "1") {
			return parse_result<T>::success(true);
		}
		if (value == "false" || value == "0") {
			return parse_result<T>::success(false);
		}
		return parse_result<T>::failure(make_bad_request("invalid_scalar", "invalid " + std::string(location_name) +
		                                                                       " '" + std::string(field_name) +
		                                                                       "': expected bool"));
	} else if constexpr (std::is_integral_v<T>) {
		T parsed {};
		const auto *begin = value.data();
		const auto *end = value.data() + value.size();
		const auto [ptr, ec] = PARSE_FLOAT(begin, end, parsed);
		if (ec == std::errc() && ptr == end) {
			return parse_result<T>::success(parsed);
		}
		return parse_result<T>::failure(make_bad_request(
		    "invalid_scalar", "invalid " + std::string(location_name) + " '" + std::string(field_name) +
		                          "': expected " + std::string(scalar_type_name<T>())));
	} else if constexpr (std::is_same_v<T, double>) {
		double parsed {};
		const auto *begin = value.data();
		const auto *end = value.data() + value.size();
		const auto [ptr, ec] = PARSE_FLOAT_FMT(begin, end, parsed, std::chars_format::general);
		if (ec == std::errc() && ptr == end && std::isfinite(parsed)) {
			return parse_result<T>::success(parsed);
		}
		return parse_result<T>::failure(make_bad_request("invalid_scalar", "invalid " + std::string(location_name) +
		                                                                       " '" + std::string(field_name) +
		                                                                       "': expected double"));
	} else {
		static_assert(always_false_v<T>, "unsupported scalar type");
	}
}

template <typename Selector, typename Service, typename Signature>
consteval bool member_signature_matches() {
	return Selector::template matches<Signature, Service>();
}

template <typename Selector, typename Service, typename... Signatures>
consteval std::size_t matching_member_signature_count(type_list<Signatures...>) {
	return (static_cast<std::size_t>(member_signature_matches<Selector, Service, Signatures>()) + ... + 0U);
}

template <typename Selector, typename Service, typename Request, typename Signature>
decltype(auto) invoke_matching_member(Service &service, Request &&request, type_list<Signature>) {
	static_assert(member_signature_matches<Selector, Service, Signature>());
	return std::invoke(Selector::template get<Signature, Service>(), service, std::forward<Request>(request));
}

template <typename Selector, typename Service, typename Request, typename Signature, typename... Signatures>
decltype(auto) invoke_matching_member(Service &service, Request &&request, type_list<Signature, Signatures...>) {
	if constexpr (member_signature_matches<Selector, Service, Signature>()) {
		return std::invoke(Selector::template get<Signature, Service>(), service, std::forward<Request>(request));
	} else {
		return invoke_matching_member<Selector>(service, std::forward<Request>(request), type_list<Signatures...> {});
	}
}

template <typename Service, typename Result, typename Request>
using endpoint_member_signatures =
    type_list<Result (Service::*)(Request), Result (Service::*)(Request) &, Result (Service::*)(Request &&),
              Result (Service::*)(Request &&) &, Result (Service::*)(const Request &),
              Result (Service::*)(const Request &) &, Result (Service::*)(const Request &&),
              Result (Service::*)(const Request &&) &, Result (Service::*)(Request) const,
              Result (Service::*)(Request) const &, Result (Service::*)(Request &&) const,
              Result (Service::*)(Request &&) const &, Result (Service::*)(const Request &) const,
              Result (Service::*)(const Request &) const &, Result (Service::*)(const Request &&) const,
              Result (Service::*)(const Request &&) const &>;

template <typename Service, typename Result, typename Request>
using endpoint_member_noexcept_signatures = type_list<
    Result (Service::*)(Request) noexcept, Result (Service::*)(Request) & noexcept,
    Result (Service::*)(Request &&) noexcept, Result (Service::*)(Request &&) & noexcept,
    Result (Service::*)(const Request &) noexcept, Result (Service::*)(const Request &) & noexcept,
    Result (Service::*)(const Request &&) noexcept, Result (Service::*)(const Request &&) & noexcept,
    Result (Service::*)(Request) const noexcept, Result (Service::*)(Request) const & noexcept,
    Result (Service::*)(Request &&) const noexcept, Result (Service::*)(Request &&) const & noexcept,
    Result (Service::*)(const Request &) const noexcept, Result (Service::*)(const Request &) const & noexcept,
    Result (Service::*)(const Request &&) const noexcept, Result (Service::*)(const Request &&) const & noexcept>;

} // namespace detail

template <typename Contract>
concept request_contract = requires(const request &req) {
	typename Contract::request_type;
	{ Contract::parse(req) } -> std::same_as<parse_result<typename Contract::request_type>>;
};

inline std::optional<std::string_view> header_value(const request &req, std::string_view name) {
	const auto it = req.find(boost::beast::string_view {name.data(), name.size()});
	if (it == req.end()) {
		return std::nullopt;
	}
	return std::string_view {it->value().data(), it->value().size()};
}

inline std::optional<binding_error> request_target_binding_error(const request &req) {
	if (!req.target_error().has_value()) {
		return std::nullopt;
	}
	return make_bad_request(req.target_error()->code, req.target_error()->message);
}

inline bool is_json_content_type(std::string_view content_type) {
	auto trim_ows = [](std::string_view value) {
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
			value.remove_suffix(1);
		}
		return value;
	};

	auto lowercase = [](std::string_view value) {
		std::string lowered;
		lowered.reserve(value.size());
		for (char c : value) {
			lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
		return lowered;
	};

	auto value = trim_ows(content_type);
	if (const auto semicolon = value.find(';'); semicolon != std::string_view::npos) {
		value = value.substr(0, semicolon);
	}
	value = trim_ows(value);
	const auto slash = value.find('/');
	if (slash == std::string_view::npos) {
		return false;
	}
	const auto type = lowercase(trim_ows(value.substr(0, slash)));
	const auto subtype = lowercase(trim_ows(value.substr(slash + 1)));
	if (type.empty() || subtype.empty() || subtype.find('/') != std::string::npos) {
		return false;
	}
	if (type == "application" && subtype == "json") {
		return true;
	}
	if (subtype.size() <= 5) {
		return false;
	}
	return subtype.ends_with("+json");
}

inline bool status_forbids_body(boost::beast::http::status status) {
	const auto code = static_cast<unsigned>(status);
	return (code >= 100 && code < 200) || code == 204 || code == 205 || code == 304;
}

template <typename T>
parse_result<T> required_path_param(const request &req, std::string_view name) {
	if (const auto target_error = request_target_binding_error(req); target_error.has_value()) {
		return parse_result<T>::failure(*target_error);
	}
	const auto value = req.path_param(name);
	if (!value.has_value()) {
		return parse_result<T>::failure(
		    make_bad_request("missing_path_parameter", "missing required path parameter '" + std::string(name) + "'"));
	}
	return detail::parse_scalar_impl<T>(*value, name, "path parameter");
}

template <typename T>
parse_result<T> required_path_param_unchecked(const request &req, std::string_view name) {
	const auto value = req.path_param(name);
	if (!value.has_value()) {
		return parse_result<T>::failure(
		    make_bad_request("missing_path_parameter", "missing required path parameter '" + std::string(name) + "'"));
	}
	return detail::parse_scalar_impl<T>(*value, name, "path parameter");
}

template <typename T>
parse_result<T> required_query_param(const request &req, std::string_view name) {
	if (const auto target_error = request_target_binding_error(req); target_error.has_value()) {
		return parse_result<T>::failure(*target_error);
	}
	const auto value = req.query_param(name);
	if (!value.has_value()) {
		return parse_result<T>::failure(make_bad_request(
		    "missing_query_parameter", "missing required query parameter '" + std::string(name) + "'"));
	}
	return detail::parse_scalar_impl<T>(*value, name, "query parameter");
}

template <typename T>
parse_result<T> required_query_param_unchecked(const request &req, std::string_view name) {
	const auto value = req.query_param(name);
	if (!value.has_value()) {
		return parse_result<T>::failure(make_bad_request(
		    "missing_query_parameter", "missing required query parameter '" + std::string(name) + "'"));
	}
	return detail::parse_scalar_impl<T>(*value, name, "query parameter");
}

template <typename T>
parse_result<std::optional<T>> optional_query_param(const request &req, std::string_view name) {
	if (const auto target_error = request_target_binding_error(req); target_error.has_value()) {
		return parse_result<std::optional<T>>::failure(*target_error);
	}
	const auto value = req.query_param(name);
	if (!value.has_value()) {
		return parse_result<std::optional<T>>::success(std::nullopt);
	}
	auto parsed = detail::parse_scalar_impl<T>(*value, name, "query parameter");
	if (!parsed.has_value()) {
		return parse_result<std::optional<T>>::failure(parsed.error());
	}
	return parse_result<std::optional<T>>::success(std::move(parsed).value());
}

template <typename T>
parse_result<std::optional<T>> optional_query_param_unchecked(const request &req, std::string_view name) {
	const auto value = req.query_param(name);
	if (!value.has_value()) {
		return parse_result<std::optional<T>>::success(std::nullopt);
	}
	auto parsed = detail::parse_scalar_impl<T>(*value, name, "query parameter");
	if (!parsed.has_value()) {
		return parse_result<std::optional<T>>::failure(parsed.error());
	}
	return parse_result<std::optional<T>>::success(std::move(parsed).value());
}

template <typename T>
parse_result<T> required_header_param(const request &req, std::string_view name) {
	const auto value = header_value(req, name);
	if (!value.has_value()) {
		return parse_result<T>::failure(
		    make_bad_request("missing_header", "missing required header '" + std::string(name) + "'"));
	}
	return detail::parse_scalar_impl<T>(*value, name, "header");
}

template <typename T>
parse_result<std::optional<T>> optional_header_param(const request &req, std::string_view name) {
	const auto value = header_value(req, name);
	if (!value.has_value()) {
		return parse_result<std::optional<T>>::success(std::nullopt);
	}
	auto parsed = detail::parse_scalar_impl<T>(*value, name, "header");
	if (!parsed.has_value()) {
		return parse_result<std::optional<T>>::failure(parsed.error());
	}
	return parse_result<std::optional<T>>::success(std::move(parsed).value());
}

template <typename T>
parse_result<T> json_body(const request &req) {
	const auto content_type = header_value(req, "Content-Type");
	if (!content_type.has_value() || !is_json_content_type(*content_type)) {
		return parse_result<T>::failure(
		    make_unsupported_media_type("unsupported_media_type", "expected Content-Type application/json"));
	}
	if (req.body().empty()) {
		return parse_result<T>::failure(make_bad_request("missing_body", "missing JSON request body"));
	}
	auto parsed_json = req.try_json_body();
	if (!parsed_json.has_value()) {
		return parse_result<T>::failure(make_bad_request("invalid_json", "invalid JSON request body"));
	}
	try {
		return parse_result<T>::success(boost::json::value_to<T>(*parsed_json));
	} catch (const std::exception &ex) {
		return parse_result<T>::failure(
		    make_bad_request("json_schema_mismatch", "JSON body schema mismatch: " + std::string(ex.what())));
	}
}

template <typename RequestContract>
parse_result<RequestContract> parse_http_request(const request &req) {
	if (const auto target_error = request_target_binding_error(req); target_error.has_value()) {
		return parse_result<RequestContract>::failure(*target_error);
	}
	return request_contract_traits<RequestContract>::parse(req);
}

template <typename Class, typename Value, auto Setter>
struct member_setter {
	using class_type = Class;
	using value_type = Value;

	static_assert(std::is_invocable_v<decltype(Setter), class_type &, value_type>,
	              "member_setter requires an invocable setter with signature compatible with (Class&, Value)");

	static void set(class_type &out, value_type value) noexcept(noexcept(std::invoke(Setter, out, std::move(value)))) {
		static_cast<void>(std::invoke(Setter, out, std::move(value)));
	}
};

template <auto Setter>
struct deduced_member_setter {
	using traits = detail::member_function_traits<decltype(Setter)>;
	using class_type = typename traits::class_type;
	using value_type = typename traits::argument_type;

	static void set(class_type &out, value_type value) noexcept(noexcept(std::invoke(Setter, out, std::move(value)))) {
		static_cast<void>(std::invoke(Setter, out, std::move(value)));
	}
};

template <typename Class, typename Value, auto ConstGetter, auto MoveGetter>
struct member_getter {
	using class_type = Class;
	using value_type = Value;

	static_assert(std::is_invocable_r_v<const value_type &, decltype(ConstGetter), const class_type &>,
	              "member_getter requires a const lvalue getter returning const Value&");
	static_assert(std::is_invocable_r_v<value_type &&, decltype(MoveGetter), class_type &&>,
	              "member_getter requires an rvalue getter returning Value&&");

	static decltype(auto) get(const class_type &value) noexcept(noexcept(std::invoke(ConstGetter, value))) {
		return std::invoke(ConstGetter, value);
	}

	static decltype(auto) get(class_type &&value) noexcept(noexcept(std::invoke(MoveGetter, std::move(value)))) {
		return std::invoke(MoveGetter, std::move(value));
	}
};

template <auto ConstGetter, auto MoveGetter = ConstGetter>
struct deduced_member_getter {
	using const_traits = detail::member_function_traits<decltype(ConstGetter)>;
	using move_traits = detail::member_function_traits<decltype(MoveGetter)>;
	using class_type = typename const_traits::class_type;
	using value_type = typename const_traits::value_type;

	static_assert(std::is_same_v<class_type, typename move_traits::class_type>,
	              "deduced_member_getter requires getters from the same class");
	static_assert(std::is_same_v<value_type, typename move_traits::value_type>,
	              "deduced_member_getter requires getters for the same value type");

	static decltype(auto) get(const class_type &value) noexcept(noexcept(std::invoke(ConstGetter, value))) {
		return std::invoke(ConstGetter, value);
	}

	static decltype(auto) get(class_type &&value) noexcept(noexcept(std::invoke(MoveGetter, std::move(value)))) {
		return std::invoke(MoveGetter, std::move(value));
	}
};

template <typename Accessor, http::fixed_string Name>
struct path_binding {
	using request_type = typename Accessor::class_type;
	using value_type = typename Accessor::value_type;

	static_assert(!detail::optional_value_traits<value_type>::is_optional,
	              "path bindings must target a non-optional member");

	static parse_result<value_type> parse(const request &req) {
		return required_path_param<value_type>(req, Name.view());
	}

	static parse_result<value_type> parse_unchecked(const request &req) {
		return required_path_param_unchecked<value_type>(req, Name.view());
	}

	static void set(request_type &out, value_type value) {
		Accessor::set(out, std::move(value));
	}
};

template <typename Accessor, http::fixed_string Name>
struct query_binding {
	using request_type = typename Accessor::class_type;
	using value_type = typename Accessor::value_type;
	using scalar_type = typename detail::optional_value_traits<value_type>::value_type;

	static parse_result<value_type> parse(const request &req) {
		if constexpr (detail::optional_value_traits<value_type>::is_optional) {
			return optional_query_param<scalar_type>(req, Name.view());
		} else {
			return required_query_param<value_type>(req, Name.view());
		}
	}

	static parse_result<value_type> parse_unchecked(const request &req) {
		if constexpr (detail::optional_value_traits<value_type>::is_optional) {
			return optional_query_param_unchecked<scalar_type>(req, Name.view());
		} else {
			return required_query_param_unchecked<value_type>(req, Name.view());
		}
	}

	static void set(request_type &out, value_type value) {
		Accessor::set(out, std::move(value));
	}
};

template <typename Accessor, http::fixed_string Name>
struct header_binding {
	using request_type = typename Accessor::class_type;
	using value_type = typename Accessor::value_type;
	using scalar_type = typename detail::optional_value_traits<value_type>::value_type;

	static parse_result<value_type> parse(const request &req) {
		if constexpr (detail::optional_value_traits<value_type>::is_optional) {
			return optional_header_param<scalar_type>(req, Name.view());
		} else {
			return required_header_param<value_type>(req, Name.view());
		}
	}

	static parse_result<value_type> parse_unchecked(const request &req) {
		return parse(req);
	}

	static void set(request_type &out, value_type value) {
		Accessor::set(out, std::move(value));
	}
};

template <typename Accessor>
struct json_body_binding {
	using request_type = typename Accessor::class_type;
	using value_type = typename Accessor::value_type;

	static_assert(!detail::optional_value_traits<value_type>::is_optional,
	              "JSON body bindings must target a non-optional member");

	static parse_result<value_type> parse(const request &req) {
		return json_body<value_type>(req);
	}

	static parse_result<value_type> parse_unchecked(const request &req) {
		return parse(req);
	}

	static void set(request_type &out, value_type value) {
		Accessor::set(out, std::move(value));
	}
};

template <typename Request, typename Value, auto Setter, http::fixed_string Name>
using path_member_binding = path_binding<member_setter<Request, Value, Setter>, Name>;

template <typename Request, typename Value, auto Setter, http::fixed_string Name>
using query_member_binding = query_binding<member_setter<Request, Value, Setter>, Name>;

template <typename Request, typename Value, auto Setter, http::fixed_string Name>
using header_member_binding = header_binding<member_setter<Request, Value, Setter>, Name>;

template <typename Request, typename Value, auto Setter>
using json_body_member_binding = json_body_binding<member_setter<Request, Value, Setter>>;

template <auto Setter, http::fixed_string Name>
using path_setter_binding = path_binding<deduced_member_setter<Setter>, Name>;

template <auto Setter, http::fixed_string Name>
using query_setter_binding = query_binding<deduced_member_setter<Setter>, Name>;

template <auto Setter, http::fixed_string Name>
using header_setter_binding = header_binding<deduced_member_setter<Setter>, Name>;

template <auto Setter>
using json_body_setter_binding = json_body_binding<deduced_member_setter<Setter>>;

template <typename Request, typename... Bindings>
struct generated_request_contract {
	using request_type = Request;

	static_assert((std::is_same_v<Request, typename Bindings::request_type> && ...),
	              "all bindings in a request contract must target the same request type");

	static parse_result<Request> parse(const request &req) {
		// this will be true if there was a parse error when converting the beast http request into
		// the warp::request object in the parser (basically when the query/path params are being
		// parsed since beast doesnt do this by default for you).
		if (const auto target_error = request_target_binding_error(req); target_error.has_value())
			return parse_result<Request>::failure(*target_error);

		Request out;
		binding_error error;
		if (!(apply_binding<Bindings>(out, req, error) && ...))
			return parse_result<Request>::failure(std::move(error));

		return parse_result<Request>::success(std::move(out));
	}

private:
	template <typename Binding>
	static bool apply_binding(Request &out, const request &req, binding_error &error) {
		auto parsed = parse_binding<Binding>(req);
		if (!parsed.has_value()) {
			error = parsed.error();
			return false;
		}
		Binding::set(out, std::move(parsed).value());
		return true;
	}

	template <typename Binding>
	static auto parse_binding(const request &req) {
		if constexpr (requires { Binding::parse_unchecked(req); }) {
			return Binding::parse_unchecked(req);
		} else {
			return Binding::parse(req);
		}
	}
};

template <typename Response>
struct empty_response_contract {
	using response_type = Response;
	static constexpr unsigned status_code = Response::status_code;
	static constexpr bool has_body = false;
};

template <typename Response, typename BodyAccessor>
struct body_response_contract {
	using response_type = Response;
	static constexpr unsigned status_code = Response::status_code;
	static constexpr bool has_body = true;

	static decltype(auto) body(const Response &value) {
		return BodyAccessor::get(value);
	}

	static decltype(auto) body(Response &&value) {
		return BodyAccessor::get(std::move(value));
	}
};

template <typename Response, typename Body, auto ConstGetter, auto MoveGetter>
using member_body_response_contract =
    body_response_contract<Response, member_getter<Response, Body, ConstGetter, MoveGetter>>;

template <auto ConstGetter, auto MoveGetter = ConstGetter>
using deduced_body_response_contract =
    body_response_contract<typename deduced_member_getter<ConstGetter, MoveGetter>::class_type,
                           deduced_member_getter<ConstGetter, MoveGetter>>;

template <typename T>
struct endpoint_response {
	boost::beast::http::status status {boost::beast::http::status::ok};
	T body;
};

template <>
struct endpoint_response<void> {
	boost::beast::http::status status {boost::beast::http::status::no_content};
};

template <typename ResponseType>
class handler_result {
public:
	handler_result(ResponseType value) : storage_(std::in_place_index<0>, std::move(value)) {
	}

	handler_result(response value) : storage_(std::in_place_index<1>, std::move(value)) {
	}

	[[nodiscard]] bool has_typed_response() const noexcept {
		return std::holds_alternative<ResponseType>(storage_);
	}

	[[nodiscard]] bool has_raw_response() const noexcept {
		return std::holds_alternative<response>(storage_);
	}

	[[nodiscard]] ResponseType &typed_response() & {
		return std::get<ResponseType>(storage_);
	}

	[[nodiscard]] const ResponseType &typed_response() const & {
		return std::get<ResponseType>(storage_);
	}

	[[nodiscard]] ResponseType &&typed_response() && {
		return std::move(std::get<ResponseType>(storage_));
	}

	[[nodiscard]] response &raw_response() & {
		return std::get<response>(storage_);
	}

	[[nodiscard]] const response &raw_response() const & {
		return std::get<response>(storage_);
	}

	[[nodiscard]] response &&raw_response() && {
		return std::move(std::get<response>(storage_));
	}

private:
	std::variant<ResponseType, response> storage_;
};

template <typename ResponseContract, typename Response>
    requires requires { typename ResponseContract::response_type; } &&
             std::same_as<typename ResponseContract::response_type, std::remove_cvref_t<Response>>
response to_http_response(Response &&typed, unsigned version = 11);

template <typename ResponseContract>
    requires(!std::is_lvalue_reference_v<ResponseContract>)
response to_http_response(ResponseContract &&typed, unsigned version = 11);

template <typename T>
response to_http_response(endpoint_response<T> typed, unsigned version = 11) {
	if (status_forbids_body(typed.status)) {
		throw std::invalid_argument("response status must not include a body");
	}
	return response_builder().status(typed.status).version(version).body(boost::json::value_from(typed.body)).build();
}

inline response to_http_response(endpoint_response<void> typed, unsigned version = 11) {
	response resp(typed.status, version);
	return resp;
}

template <typename ResponseType>
response to_http_response(handler_result<ResponseType> typed, unsigned version = 11) {
	if (typed.has_raw_response()) {
		return std::move(typed).raw_response();
	}
	return warp::codegen::to_http_response(std::move(typed).typed_response(), version);
}

template <typename ResponseContract, typename ResponseType>
    requires requires { typename ResponseContract::response_type; } &&
             std::same_as<typename ResponseContract::response_type, ResponseType>
response to_http_response(handler_result<ResponseType> typed, unsigned version = 11) {
	if (typed.has_raw_response()) {
		return std::move(typed).raw_response();
	}
	return warp::codegen::to_http_response<ResponseContract>(std::move(typed).typed_response(), version);
}

template <typename ResponseContract>
response to_http_response(const ResponseContract &typed, unsigned version = 11) {
	using traits = response_contract_traits<ResponseContract>;

	if constexpr (traits::has_body) {
		if (status_forbids_body(static_cast<boost::beast::http::status>(traits::status_code))) {
			throw std::invalid_argument("response status must not include a body");
		}
		return response_builder()
		    .status(traits::status_code)
		    .version(version)
		    .body(boost::json::value_from(traits::body(typed)))
		    .build();
	} else {
		response resp(static_cast<boost::beast::http::status>(traits::status_code), version);
		return resp;
	}
}

template <typename ResponseContract, typename Response>
    requires requires { typename ResponseContract::response_type; } &&
             std::same_as<typename ResponseContract::response_type, std::remove_cvref_t<Response>>
response to_http_response(Response &&typed, unsigned version) {
	if constexpr (ResponseContract::has_body) {
		if (status_forbids_body(static_cast<boost::beast::http::status>(ResponseContract::status_code))) {
			throw std::invalid_argument("response status must not include a body");
		}
		return response_builder()
		    .status(ResponseContract::status_code)
		    .version(version)
		    .body(boost::json::value_from(ResponseContract::body(std::forward<Response>(typed))))
		    .build();
	} else {
		response resp(static_cast<boost::beast::http::status>(ResponseContract::status_code), version);
		return resp;
	}
}

template <typename ResponseContract>
    requires(!std::is_lvalue_reference_v<ResponseContract>)
response to_http_response(ResponseContract &&typed, unsigned version) {
	using response_type = std::remove_cvref_t<ResponseContract>;
	using traits = response_contract_traits<response_type>;

	if constexpr (traits::has_body) {
		if (status_forbids_body(static_cast<boost::beast::http::status>(traits::status_code))) {
			throw std::invalid_argument("response status must not include a body");
		}
		return response_builder()
		    .status(traits::status_code)
		    .version(version)
		    .body(boost::json::value_from(traits::body(std::forward<ResponseContract>(typed))))
		    .build();
	} else {
		response resp(static_cast<boost::beast::http::status>(traits::status_code), version);
		return resp;
	}
}

template <typename ResponseType, typename Invocable>
awaitable<ResponseType> invoke_user_handler(Invocable &&invocable) {
	using result_type = std::remove_cvref_t<std::invoke_result_t<Invocable>>;
	if constexpr (std::is_same_v<result_type, ResponseType>) {
		co_return std::invoke(std::forward<Invocable>(invocable));
	} else if constexpr (std::is_same_v<result_type, awaitable<ResponseType>>) {
		co_return co_await std::invoke(std::forward<Invocable>(invocable));
	} else {
		static_assert(detail::always_false_v<result_type>,
		              "handler must return ResponseType or warp::awaitable<ResponseType>");
	}
}

inline response normalize_handler_response(response resp, unsigned version, bool keep_alive) {
	resp.version(version);
	resp.keep_alive(keep_alive);
	return resp;
}

template <typename ResponseType, typename RequestType, typename Service, typename Selector>
decltype(auto) invoke_endpoint_handler_overload(Service &service, RequestType &&request) {
	using sync_signatures = detail::endpoint_member_signatures<Service, ResponseType, RequestType>;
	using sync_noexcept_signatures = detail::endpoint_member_noexcept_signatures<Service, ResponseType, RequestType>;
	using async_signatures = detail::endpoint_member_signatures<Service, awaitable<ResponseType>, RequestType>;
	using async_noexcept_signatures =
	    detail::endpoint_member_noexcept_signatures<Service, awaitable<ResponseType>, RequestType>;
	using mixed_sync_signatures =
	    detail::endpoint_member_signatures<Service, handler_result<ResponseType>, RequestType>;
	using mixed_sync_noexcept_signatures =
	    detail::endpoint_member_noexcept_signatures<Service, handler_result<ResponseType>, RequestType>;
	using mixed_async_signatures =
	    detail::endpoint_member_signatures<Service, awaitable<handler_result<ResponseType>>, RequestType>;
	using mixed_async_noexcept_signatures =
	    detail::endpoint_member_noexcept_signatures<Service, awaitable<handler_result<ResponseType>>, RequestType>;

	constexpr auto sync_match_count = detail::matching_member_signature_count<Selector, Service>(sync_signatures {});
	constexpr auto sync_noexcept_match_count =
	    detail::matching_member_signature_count<Selector, Service>(sync_noexcept_signatures {});
	constexpr auto async_match_count = detail::matching_member_signature_count<Selector, Service>(async_signatures {});
	constexpr auto async_noexcept_match_count =
	    detail::matching_member_signature_count<Selector, Service>(async_noexcept_signatures {});
	constexpr auto mixed_sync_match_count =
	    detail::matching_member_signature_count<Selector, Service>(mixed_sync_signatures {});
	constexpr auto mixed_sync_noexcept_match_count =
	    detail::matching_member_signature_count<Selector, Service>(mixed_sync_noexcept_signatures {});
	constexpr auto mixed_async_match_count =
	    detail::matching_member_signature_count<Selector, Service>(mixed_async_signatures {});
	constexpr auto mixed_async_noexcept_match_count =
	    detail::matching_member_signature_count<Selector, Service>(mixed_async_noexcept_signatures {});
	constexpr auto total_match_count =
	    sync_match_count + async_match_count + mixed_sync_match_count + mixed_async_match_count;

	static_assert(total_match_count > 0,
	              "generated endpoint handler resolution could not find an overload matching the endpoint request/"
	              "response contract or warp::codegen::handler_result<ResponseType>");
	static_assert(total_match_count == 1,
	              "generated endpoint handler resolution is ambiguous: multiple overloads match the endpoint request/"
	              "response contract or warp::codegen::handler_result<ResponseType>");

	if constexpr (sync_match_count == 1) {
		if constexpr (sync_noexcept_match_count == 1) {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                sync_noexcept_signatures {});
		} else {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                sync_signatures {});
		}
	} else if constexpr (async_match_count == 1) {
		if constexpr (async_noexcept_match_count == 1) {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                async_noexcept_signatures {});
		} else {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                async_signatures {});
		}
	} else if constexpr (mixed_sync_match_count == 1) {
		if constexpr (mixed_sync_noexcept_match_count == 1) {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                mixed_sync_noexcept_signatures {});
		} else {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                mixed_sync_signatures {});
		}
	} else {
		if constexpr (mixed_async_noexcept_match_count == 1) {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                mixed_async_noexcept_signatures {});
		} else {
			return detail::invoke_matching_member<Selector>(service, std::forward<RequestType>(request),
			                                                mixed_async_signatures {});
		}
	}
}

template <typename ResponseType, typename Service, typename HandlerFn, typename RequestType>
concept endpoint_handler = requires(HandlerFn handler_fn, Service &service, RequestType request) {
	{ std::invoke(handler_fn, service, std::move(request)) } -> std::same_as<ResponseType>;
} || requires(HandlerFn handler_fn, Service &service, RequestType request) {
	{ std::invoke(handler_fn, service, std::move(request)) } -> std::same_as<awaitable<ResponseType>>;
} || requires(HandlerFn handler_fn, Service &service, RequestType request) {
	{ std::invoke(handler_fn, service, std::move(request)) } -> std::same_as<handler_result<ResponseType>>;
} || requires(HandlerFn handler_fn, Service &service, RequestType request) {
	{ std::invoke(handler_fn, service, std::move(request)) } -> std::same_as<awaitable<handler_result<ResponseType>>>;
};

template <request_contract RequestContract, typename ResponseType, typename Service, typename MemberFn>
    requires endpoint_handler<ResponseType, Service, MemberFn, typename RequestContract::request_type>
auto bind_endpoint(std::shared_ptr<Service> service, MemberFn member_fn) {
	using request_type = typename RequestContract::request_type;
	using handler_return = std::remove_cvref_t<std::invoke_result_t<MemberFn, Service &, request_type>>;

	if constexpr (std::is_same_v<handler_return, ResponseType>) {
		return [service = std::move(service), member_fn](warp::request req) mutable -> warp::response {
			const auto version = req.version();
			const auto keep_alive = req.keep_alive();

			auto typed_request = RequestContract::parse(req);
			if (!typed_request.has_value()) {
				return warp::codegen::normalize_handler_response(
				    warp::codegen::to_error_response(typed_request.error(), version), version, keep_alive);
			}

			auto *service_ptr = service.get();
			auto typed_response = std::invoke(member_fn, *service_ptr, std::move(typed_request).value());

			return warp::codegen::normalize_handler_response(
			    warp::codegen::to_http_response(std::move(typed_response), version), version, keep_alive);
		};
	} else if constexpr (std::is_same_v<handler_return, handler_result<ResponseType>>) {
		return [service = std::move(service), member_fn](warp::request req) mutable -> warp::response {
			const auto version = req.version();
			const auto keep_alive = req.keep_alive();

			auto typed_request = RequestContract::parse(req);
			if (!typed_request.has_value()) {
				return warp::codegen::normalize_handler_response(
				    warp::codegen::to_error_response(typed_request.error(), version), version, keep_alive);
			}

			auto *service_ptr = service.get();
			return warp::codegen::normalize_handler_response(
			    warp::codegen::to_http_response(std::invoke(member_fn, *service_ptr, std::move(typed_request).value()),
			                                    version),
			    version, keep_alive);
		};
	} else {
		static_assert(std::is_same_v<handler_return, awaitable<ResponseType>> ||
		                  std::is_same_v<handler_return, awaitable<handler_result<ResponseType>>>,
		              "bind_endpoint handlers must return ResponseType, "
		              "warp::codegen::handler_result<ResponseType>, warp::awaitable<ResponseType>, or "
		              "warp::awaitable<warp::codegen::handler_result<ResponseType>>");
		return [service = std::move(service), member_fn](warp::request req) -> warp::awaitable<warp::response> {
			const auto version = req.version();
			const auto keep_alive = req.keep_alive();

			auto typed_request = RequestContract::parse(req);
			if (!typed_request.has_value()) {
				co_return warp::codegen::normalize_handler_response(
				    warp::codegen::to_error_response(typed_request.error(), version), version, keep_alive);
			}

			auto *service_ptr = service.get();
			if constexpr (std::is_same_v<handler_return, awaitable<ResponseType>>) {
				auto typed_response = co_await warp::codegen::invoke_user_handler<ResponseType>(
				    [service_ptr, member_fn, typed_request = std::move(typed_request).value()]() mutable {
					    return std::invoke(member_fn, *service_ptr, std::move(typed_request));
				    });

				co_return warp::codegen::normalize_handler_response(
				    warp::codegen::to_http_response(std::move(typed_response), version), version, keep_alive);
			} else {
				auto mixed_response = co_await std::invoke(member_fn, *service_ptr, std::move(typed_request).value());
				co_return warp::codegen::normalize_handler_response(
				    warp::codegen::to_http_response(std::move(mixed_response), version), version, keep_alive);
			}
		};
	}
}

template <request_contract RequestContract, typename ResponseContract, typename Service, typename Selector>
auto bind_generated_endpoint(std::shared_ptr<Service> service) {
	using request_type = typename RequestContract::request_type;
	using response_type = typename ResponseContract::response_type;
	using handler_return =
	    std::remove_cvref_t<decltype(invoke_endpoint_handler_overload<response_type, request_type, Service, Selector>(
	        std::declval<Service &>(), std::declval<request_type>()))>;

	if constexpr (std::is_same_v<handler_return, response_type>) {
		return [service = std::move(service)](warp::request req) mutable -> warp::response {
			const auto version = req.version();
			const auto keep_alive = req.keep_alive();

			auto typed_request = RequestContract::parse(req);
			if (!typed_request.has_value()) {
				return warp::codegen::normalize_handler_response(
				    warp::codegen::to_error_response(typed_request.error(), version), version, keep_alive);
			}

			auto *service_ptr = service.get();
			auto typed_response = invoke_endpoint_handler_overload<response_type, request_type, Service, Selector>(
			    *service_ptr, std::move(typed_request).value());

			return warp::codegen::normalize_handler_response(
			    warp::codegen::to_http_response<ResponseContract>(std::move(typed_response), version), version,
			    keep_alive);
		};
	} else if constexpr (std::is_same_v<handler_return, handler_result<response_type>>) {
		return [service = std::move(service)](warp::request req) mutable -> warp::response {
			const auto version = req.version();
			const auto keep_alive = req.keep_alive();

			auto typed_request = RequestContract::parse(req);
			if (!typed_request.has_value()) {
				return warp::codegen::normalize_handler_response(
				    warp::codegen::to_error_response(typed_request.error(), version), version, keep_alive);
			}

			auto *service_ptr = service.get();
			auto mixed_response = invoke_endpoint_handler_overload<response_type, request_type, Service, Selector>(
			    *service_ptr, std::move(typed_request).value());
			return warp::codegen::normalize_handler_response(
			    warp::codegen::to_http_response<ResponseContract>(std::move(mixed_response), version), version,
			    keep_alive);
		};
	} else {
		static_assert(std::is_same_v<handler_return, awaitable<response_type>> ||
		                  std::is_same_v<handler_return, awaitable<handler_result<response_type>>>,
		              "generated endpoint handlers must return ResponseType, "
		              "warp::codegen::handler_result<ResponseType>, warp::awaitable<ResponseType>, or "
		              "warp::awaitable<warp::codegen::handler_result<ResponseType>>");
		return [service = std::move(service)](warp::request req) -> warp::awaitable<warp::response> {
			const auto version = req.version();
			const auto keep_alive = req.keep_alive();

			auto typed_request = RequestContract::parse(req);
			if (!typed_request.has_value()) {
				co_return warp::codegen::normalize_handler_response(
				    warp::codegen::to_error_response(typed_request.error(), version), version, keep_alive);
			}

			auto *service_ptr = service.get();
			if constexpr (std::is_same_v<handler_return, awaitable<response_type>>) {
				auto typed_response =
				    co_await invoke_endpoint_handler_overload<response_type, request_type, Service, Selector>(
				        *service_ptr, std::move(typed_request).value());

				co_return warp::codegen::normalize_handler_response(
				    warp::codegen::to_http_response<ResponseContract>(std::move(typed_response), version), version,
				    keep_alive);
			} else {
				auto mixed_response =
				    co_await invoke_endpoint_handler_overload<response_type, request_type, Service, Selector>(
				        *service_ptr, std::move(typed_request).value());
				co_return warp::codegen::normalize_handler_response(
				    warp::codegen::to_http_response<ResponseContract>(std::move(mixed_response), version), version,
				    keep_alive);
			}
		};
	}
}

template <typename Service, typename Route, request_contract RequestContract, typename ResponseType, auto MemberFn>
struct endpoint_binding {
	static void register_route(http::server_builder &builder, const std::shared_ptr<Service> &service) {
		builder.route(Route {}, bind_endpoint<RequestContract, ResponseType>(service, MemberFn));
	}
};

template <typename Service, typename Route, request_contract RequestContract, typename ResponseContract,
          typename Selector>
struct generated_endpoint_binding {
	static void register_route(http::server_builder &builder, const std::shared_ptr<Service> &service) {
		builder.route(Route {}, bind_generated_endpoint<RequestContract, ResponseContract, Service, Selector>(service));
	}
};

template <typename Service, typename... Endpoints>
class generated_resource {
public:
	explicit generated_resource(std::shared_ptr<Service> service) : service_(std::move(service)) {
		if (!service_) {
			throw std::invalid_argument("service must not be null");
		}
	}

	void register_routes(http::server_builder &builder) const {
		(Endpoints::register_route(builder, service_), ...);
	}

private:
	std::shared_ptr<Service> service_;
};

template <typename... Resources>
void register_resources(http::server_builder &builder, Resources &...resources) {
	(builder.register_resource(resources), ...);
}

} // namespace warp::codegen
