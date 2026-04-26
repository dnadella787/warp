#pragma once

#include <memory>
#include <string>

#include "warp/http/event_loop_mode.hpp"

namespace warp::server {

class server_builder;

class server {
	// We use impl_base because server_impl is a template class whose full type is not
	// known at compile time since server is not template based. We have no way to store
	// std::shared_ptr<server_impl<Mode>> since Mode is unknown here so we use type
	// erasure via impl_base to store the actual internal impl obj
	struct impl_base {
		virtual ~impl_base() = default;
		virtual void run(bool blocking) = 0;
		virtual void stop() = 0;
	};

	template <http::event_loop_mode Mode>
	class server_impl;

public:
	server();
	~server();
	server(server &&) noexcept;
	server &operator=(server &&) noexcept;

	void run(bool blocking = true);
	void stop();

	// note that controller keeps a reference to the impl and not the server
	// object itself since server is a stack allocated obj, we do not keep any shared_ptr
	// references to it anywhere. This way we can entirely decouple the lifetimes of the
	// server and controller objects
	class controller {
	public:
		void stop();

	private:
		friend class server;
		explicit controller(const std::shared_ptr<impl_base> &impl);
		std::weak_ptr<impl_base> impl_;
	};

	[[nodiscard]] controller get_controller() const;

private:
	friend class server_builder;
	explicit server(std::shared_ptr<impl_base> impl);

	// we use std::shared_ptr instead of std::unique_ptr so controller can get a std::weak_ptr to impl
	std::shared_ptr<impl_base> impl_;
};

} // namespace warp::server
