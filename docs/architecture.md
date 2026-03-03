# Architecture Overview

Warp is split into a set of internal libraries exposed through headers under `include/warp`:

- `net/core/io_context_pool.hpp`: Owns the thread-aware Boost.Asio `io_context` pool and exposes scheduling primitives.
- `net/http.hpp`: Defines request/response wrappers, HTTP method enums, and error translation helpers.
- `net/router.hpp`: Stores thread-safe routing tables that map paths to handlers.
- `net/util/status.hpp`: Provides status codes, error metadata, and shared utility helpers.
- `http/server.hpp`: The public façade that composes the internal components and implements the Boost-backed server runtime.

Boost dependencies are linked privately inside the `warp_http` target. Consumers include only headers such as `warp/http/server.hpp` and never touch Boost types directly.

`server::run()` is a blocking call; it drives the accept loop on the calling thread while worker threads are provided by the internal `io_context_pool`.

### Request Lifecycle
1. Incoming sockets are accepted on a dedicated acceptor running in its own `io_context`.
2. Connections are handed off to the `io_context_pool`, enabling configurable worker threads.
3. Boost.Beast parses incoming bytes into HTTP requests, which are translated into Warp's value-based `request`.
4. The router resolves a handler and executes it (coroutines support is planned), producing a Warp `response`.
5. Responses are serialized back with Boost.Beast and sent on the socket. Keep-alive is disabled by default for clarity in the initial prototype.

### Future Work
- Coroutine support (`co_await` handlers) and structured concurrency controls.
- Advanced routing (wildcards, parameter extraction, middleware chains).
- Observability hooks (metrics, tracing, structured logs).
- Memory resource pooling and allocator customization.
