#include "warp/server/server.hpp"

#include <algorithm>
#include <memory>

#include "server_impl.hpp"

namespace warp::server {

server::server() = default;
server::~server() {
	stop();
}

server::server(std::shared_ptr<impl_base> impl) : impl_(std::move(impl)) {
}
server::server(server &&) noexcept = default;
server &server::operator=(server &&) noexcept = default;

void server::run(bool blocking) {
	if (impl_)
		impl_->run(blocking);
}

void server::stop() {
	if (impl_)
		impl_->stop();
}

server::controller server::get_controller() const {
	return controller {impl_};
}

server::controller::controller(const std::shared_ptr<impl_base> &impl) : impl_(impl) {
}

void server::controller::stop() {
	if (auto locked = impl_.lock())
		locked->stop();
}

} // namespace warp::server
