#include "transport.h"

#include <stdexcept>

namespace warp::server {

namespace {

template <typename Stream>
void close_lowest_layer(Stream &stream) {
	beast::error_code ignored;
	beast::get_lowest_layer(stream).socket().close(ignored);
}

} // namespace

plain_session_transport::stream_type plain_session_transport::make_stream(tcp::socket &&socket) const {
	return beast::tcp_stream(std::move(socket));
}

void plain_session_transport::abort(stream_type &stream) {
	close_lowest_layer(stream);
}

tls_session_transport::tls_session_transport(std::shared_ptr<ssl::context> ctx) : ctx_(std::move(ctx)) {
}

tls_session_transport::stream_type tls_session_transport::make_stream(tcp::socket &&socket) const {
	return stream_type(std::move(socket), *ctx_);
}

void tls_session_transport::abort(stream_type &stream) {
	close_lowest_layer(stream);
}

} // namespace warp::server
