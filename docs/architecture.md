# Architecture Overview

Warp is split into a set of internal libraries exposed through headers under `include/warp`. Public headers deliberately avoid leaking Boost symbols; anything that needs Boost now lives behind private PIMPLs or façade types.

- `net/core/io_context_pool.hpp`: Provides an opaque executor pool API; the underlying Boost.Asio contexts are hidden behind a private implementation and accessed only through `detail::executor_access` within library sources.
- `net/http.hpp`: Defines request/response wrappers, HTTP method enums, query/path parameter helpers, and a Boost-free `json_value` façade that internally marshals to Boost.JSON in `src/net/http/request.cpp`.
- `net/router.hpp`: Stores thread-safe routing tables that map literal and `{param}` patterns to handlers.
- `http/server.hpp`: The public façade that composes the internal components. The accept loop now lives in `server_impl.cpp`, while per-connection state is handled by `detail::session` in its own translation unit (`session.cpp`), keeping Boost.Beast specifics out of headers.

Boost modules (Asio, Beast, JSON) are linked privately by the `warp_http` target so downstream applications never need to include or link against Boost directly.

`server::run()` is a blocking call; it drives the accept loop on the calling thread while worker threads are provided by the internal `io_context_pool`.

### Request Lifecycle
1. Incoming sockets are accepted on a dedicated acceptor running in its own `io_context`.
2. Connections are handed off to the `io_context_pool`, enabling configurable worker threads.
3. Boost.Beast parses incoming bytes into HTTP requests, which are translated into Warp's value-based `request` (including parsed query parameters and lazy `json_value` helpers that defer Boost usage to the `.cpp` layer).
4. The router resolves a handler, captures any `{param}` segments into the `request`, and executes the handler (coroutines support is planned), producing a Warp `response`.
5. Responses are serialized back with Boost.Beast and sent on the socket. Keep-alive is disabled by default for clarity in the initial prototype.

### Future Work
- Coroutine support (`co_await` handlers) and structured concurrency controls.
- Advanced routing (wildcards, parameter extraction, middleware chains).
- Observability hooks (metrics, tracing, structured logs).
- Memory resource pooling and allocator customization.
