#pragma once

#include "warp/http/response_builder.hpp"
#include "warp/http/server.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace warp::codegen {

struct binding_error {
	std::string message;
};

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

inline response to_bad_request_response(const binding_error &error, unsigned version = 11) {
	return response_builder()
	    .status(boost::beast::http::status::bad_request)
	    .version(version)
	    .body(body_builder().set("error", error.message).build())
	    .build();
}

template <typename T>
struct request_contract_traits;

template <typename T>
struct response_contract_traits;

namespace detail {

template <typename T>
inline constexpr bool always_false_v = false;

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
		return parse_result<T>::failure(binding_error {"invalid " + std::string(location_name) + " '" +
		                                               std::string(field_name) + "': expected bool"});
	} else if constexpr (std::is_integral_v<T>) {
		T parsed {};
		const auto *begin = value.data();
		const auto *end = value.data() + value.size();
		const auto [ptr, ec] = std::from_chars(begin, end, parsed);
		if (ec == std::errc() && ptr == end) {
			return parse_result<T>::success(parsed);
		}
		return parse_result<T>::failure(binding_error {"invalid " + std::string(location_name) + " '" +
		                                               std::string(field_name) + "': expected " +
		                                               std::string(scalar_type_name<T>())});
	} else if constexpr (std::is_same_v<T, double>) {
		std::string text(value);
		char *end = nullptr;
		const double parsed = std::strtod(text.c_str(), &end);
		if (end != nullptr && *end == '\0') {
			return parse_result<T>::success(parsed);
		}
		return parse_result<T>::failure(binding_error {"invalid " + std::string(location_name) + " '" +
		                                               std::string(field_name) + "': expected double"});
	} else {
		static_assert(always_false_v<T>, "unsupported scalar type");
	}
}

} // namespace detail

inline std::optional<std::string_view> header_value(const request &req, std::string_view name) {
	const auto it = req.find(boost::beast::string_view {name.data(), name.size()});
	if (it == req.end()) {
		return std::nullopt;
	}
	return std::string_view {it->value().data(), it->value().size()};
}

inline bool is_json_content_type(std::string_view content_type) {
	std::string lowered;
	lowered.reserve(content_type.size());
	for (char c : content_type) {
		lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return lowered.rfind("application/json", 0) == 0 || lowered.find("+json") != std::string::npos;
}

template <typename T>
parse_result<T> required_path_param(const request &req, std::string_view name) {
	const auto value = req.path_param(name);
	if (!value.has_value()) {
		return parse_result<T>::failure(binding_error {"missing required path parameter '" + std::string(name) + "'"});
	}
	return detail::parse_scalar_impl<T>(*value, name, "path parameter");
}

template <typename T>
parse_result<T> required_query_param(const request &req, std::string_view name) {
	const auto value = req.query_param(name);
	if (!value.has_value()) {
		return parse_result<T>::failure(binding_error {"missing required query parameter '" + std::string(name) + "'"});
	}
	return detail::parse_scalar_impl<T>(*value, name, "query parameter");
}

template <typename T>
parse_result<std::optional<T>> optional_query_param(const request &req, std::string_view name) {
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
		return parse_result<T>::failure(binding_error {"missing required header '" + std::string(name) + "'"});
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
	if (req.body().empty()) {
		return parse_result<T>::failure(binding_error {"missing JSON request body"});
	}
	const auto content_type = header_value(req, "Content-Type");
	if (!content_type.has_value() || !is_json_content_type(*content_type)) {
		return parse_result<T>::failure(binding_error {"expected Content-Type application/json"});
	}
	auto parsed_json = req.try_json_body();
	if (!parsed_json.has_value()) {
		return parse_result<T>::failure(binding_error {"invalid JSON request body"});
	}
	try {
		return parse_result<T>::success(boost::json::value_to<T>(*parsed_json));
	} catch (const std::exception &ex) {
		return parse_result<T>::failure(binding_error {"JSON body schema mismatch: " + std::string(ex.what())});
	}
}

template <typename RequestContract>
parse_result<RequestContract> parse_http_request(const request &req) {
	return request_contract_traits<RequestContract>::parse(req);
}

template <typename T>
struct endpoint_response {
	boost::beast::http::status status {boost::beast::http::status::ok};
	T body;
};

template <>
struct endpoint_response<void> {
	boost::beast::http::status status {boost::beast::http::status::no_content};
};

template <typename T>
response to_http_response(endpoint_response<T> typed, unsigned version = 11) {
	return response_builder().status(typed.status).version(version).body(boost::json::value_from(typed.body)).build();
}

inline response to_http_response(endpoint_response<void> typed, unsigned version = 11) {
	response resp(typed.status, version);
	return resp;
}

template <typename ResponseContract>
response to_http_response(const ResponseContract &typed, unsigned version = 11) {
	using traits = response_contract_traits<ResponseContract>;

	if constexpr (traits::has_body) {
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

template <typename... Resources>
void register_resources(warp::http::server_builder &builder, Resources &...resources) {
	(builder.register_resource(resources), ...);
}

} // namespace warp::codegen
