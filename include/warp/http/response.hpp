#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <boost/beast/http.hpp>

#include "warp/http/body_builder.hpp"

namespace warp::http {

using beast_response = boost::beast::http::response<boost::beast::http::string_body>;

class response : public beast_response {
public:
	response() = default;
	response(boost::beast::http::status status, unsigned version) : beast_response(status, version) {
	}
	response(const beast_response &other) : beast_response(other) {
	}
	response(beast_response &&other) noexcept : beast_response(std::move(other)) {
	}

	response &operator=(const beast_response &other) {
		beast_response::operator=(other);
		return *this;
	}

	response &operator=(beast_response &&other) noexcept {
		beast_response::operator=(std::move(other));
		return *this;
	}

	static response ok(std::string body = body_builder().build(), std::string_view content_type = "application/json");
	static response ok(const char *body, std::string_view content_type = "application/json");
	static response ok(boost::json::value body);
	static response created(std::string body = body_builder().build(),
	                        std::string_view content_type = "application/json");
	static response created(const char *body, std::string_view content_type = "application/json");
	static response created(boost::json::value body);
	static response accepted(std::string body = body_builder().build(),
	                         std::string_view content_type = "application/json");
	static response accepted(const char *body, std::string_view content_type = "application/json");
	static response accepted(boost::json::value body);
	static response no_content();
	static response bad_request(std::string error = "Bad Request");
	static response unauthorized(std::string error = "Unauthorized");
	static response forbidden(std::string error = "Forbidden");
	static response not_found(std::string error = "Not Found");
	static response conflict(std::string error = "Conflict");
	static response server_error(std::string error = "Internal Server Error");

private:
	static response make(boost::beast::http::status status, std::string body, std::string_view content_type);
	static response make_json(boost::beast::http::status status, boost::json::value body);
	static response make_error(boost::beast::http::status status, std::string error);
};

// note we do not need resp.prepare_payload() here b/c http_session::on_handle_complete() does it for us
inline response response::make(boost::beast::http::status status, std::string body, std::string_view content_type) {
	response resp {status, 11};
	if (!content_type.empty()) {
		resp.set(boost::beast::http::field::content_type, content_type);
	}
	resp.body() = std::move(body);
	return resp;
}

inline response response::make_json(boost::beast::http::status status, boost::json::value body) {
	return make(status, boost::json::serialize(body), "application/json");
}

inline response response::make_error(boost::beast::http::status status, std::string error) {
	return make(status, body_builder().set("error", std::move(error)).build(), "application/json");
}

inline response response::ok(std::string body, std::string_view content_type) {
	return make(boost::beast::http::status::ok, std::move(body), content_type);
}

inline response response::ok(const char *body, std::string_view content_type) {
	return make(boost::beast::http::status::ok, body == nullptr ? body_builder().build() : std::string {body},
	            content_type);
}

inline response response::ok(boost::json::value body) {
	return make_json(boost::beast::http::status::ok, std::move(body));
}

inline response response::created(std::string body, std::string_view content_type) {
	return make(boost::beast::http::status::created, std::move(body), content_type);
}

inline response response::created(const char *body, std::string_view content_type) {
	return make(boost::beast::http::status::created, body == nullptr ? body_builder().build() : std::string {body},
	            content_type);
}

inline response response::created(boost::json::value body) {
	return make_json(boost::beast::http::status::created, std::move(body));
}

inline response response::accepted(std::string body, std::string_view content_type) {
	return make(boost::beast::http::status::accepted, std::move(body), content_type);
}

inline response response::accepted(const char *body, std::string_view content_type) {
	return make(boost::beast::http::status::accepted, body == nullptr ? body_builder().build() : std::string {body},
	            content_type);
}

inline response response::accepted(boost::json::value body) {
	return make_json(boost::beast::http::status::accepted, std::move(body));
}

inline response response::no_content() {
	return make(boost::beast::http::status::no_content, {}, {});
}

inline response response::bad_request(std::string error) {
	return make_error(boost::beast::http::status::bad_request, std::move(error));
}

inline response response::unauthorized(std::string error) {
	return make_error(boost::beast::http::status::unauthorized, std::move(error));
}

inline response response::forbidden(std::string error) {
	return make_error(boost::beast::http::status::forbidden, std::move(error));
}

inline response response::not_found(std::string error) {
	return make_error(boost::beast::http::status::not_found, std::move(error));
}

inline response response::conflict(std::string error) {
	return make_error(boost::beast::http::status::conflict, std::move(error));
}

inline response response::server_error(std::string error) {
	return make_error(boost::beast::http::status::internal_server_error, std::move(error));
}

} // namespace warp::http
