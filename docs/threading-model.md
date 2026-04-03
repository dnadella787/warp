# Warp Threading Model

## Server Execution

Warp's HTTP server owns one `boost::asio::io_context` inside `server::impl`.

- `worker_threads(count)` controls how many background threads call `io_context::run()`
- the effective pool size is `max(1, count)`
- `server::run(true)` also runs the `io_context` on the caller thread
- `server::run(false)` leaves the work to the spawned worker threads only

All HTTP listeners, sessions, and route handlers execute on that shared `io_context` unless work is explicitly moved to another executor.

## Event Loop Selection

Warp now supports two internal HTTP event-loop implementations:

- `warp::event_loop_mode::callbacks`
- `warp::event_loop_mode::coroutines`

The mode is selected through `server_builder::event_loop(...)`. The default is `callbacks`.

At runtime, `server::impl` constructs either:

- [listener.cpp](/Users/dnadella/Projects/warp/src/http/listener/listener.cpp) plus [http_session.cpp](/Users/dnadella/Projects/warp/src/http/session/http_session.cpp)
- [coroutine_listener.cpp](/Users/dnadella/Projects/warp/src/http/listener/coroutine_listener.cpp) plus [coroutine_http_session.cpp](/Users/dnadella/Projects/warp/src/http/session/coroutine_http_session.cpp)

The public route API is the same in both modes.

## Accept Loop

Both listener implementations own one `boost::asio::ip::tcp::acceptor`.

- the acceptor is created on `boost::asio::make_strand(io_context)`
- accepts are therefore serialized through the acceptor strand
- each accepted socket is rebound to its own strand with `boost::asio::make_strand(ioc_)`

That per-connection strand guarantees serialized access to a given session's socket state even when many threads are servicing the shared `io_context`.

### Callback Listener

The callback listener:

- starts from `listener::run()`
- uses `boost::asio::dispatch(...)` onto the acceptor executor
- chains `async_accept(...)` through `do_accept()` and `on_accept(...)`

### Coroutine Listener

The coroutine listener:

- starts from `coroutine_listener::run()`
- launches `accept_loop()` with `boost::asio::co_spawn(...)`
- repeatedly `co_await`s `async_accept(...)`

## Session Execution

Each connection owns exactly one HTTP session object on its own socket strand.

Shared behavior across both session implementations:

- requests are read asynchronously with Beast
- route handlers are normalized to the internal shape `warp::awaitable<warp::response>(warp::request&&)`
- multiple requests may be in flight on the same connection up to the internal pipeline limit
- responses are buffered and written strictly in request order
- once `Connection: close` is observed, Warp stops accepting new requests on that connection and drains what is already accepted

### Callback Session

The callback session uses explicit state flags and completion handlers.

Important details:

- `http_session::maybe_read()` starts a new read only when:
  - shutdown has not started
  - the session is still accepting new requests
  - no read is already in progress
  - the connection has not reached the pipeline limit
- `http_session::on_read(...)` wraps the parsed Beast request as `warp::request`
- the matched route handler is launched with `boost::asio::co_spawn(...)`
- `on_handler_complete(...)` stores the finished response
- `do_write()` and `on_write(...)` flush responses in sequence order

This mode uses callbacks for the accept/session event loop, but route handlers may still be coroutines.

### Coroutine Session

The coroutine session moves the session control flow itself into coroutines.

Important details:

- `start()` launches `read_loop()` and `write_loop()` with `co_spawn(...)`
- `read_loop()` keeps reading requests until shutdown, `Connection: close`, or the pipeline limit blocks further reads
- matched handlers are still launched independently with `co_spawn(...)`
- completed responses are inserted into the ordered response map
- `write_loop()` waits for the next sequence-ready response and `co_await`s `async_write(...)`
- `steady_timer` instances are used as wake signals so the read loop can pause at the pipeline limit and the write loop can sleep until a response becomes ready

This keeps the session logic itself sequential without changing user-facing handler code.

## Request and Route Handling

Incoming Beast requests are wrapped as `warp::request`.

The current `warp::request` type:

- inherits from `boost::beast::http::request<boost::beast::http::string_body>`
- parses the request target into:
  - `path()`
  - `query_params()`
  - `query_param(...)`
- stores route-specific path params injected later by the registry
- exposes `json_body()` and `try_json_body()` helpers

The registry matches only against the request path and fills `path_param(...)` values on a successful route match.

Public route handlers may return either:

- `warp::response`
- `warp::awaitable<warp::response>`

`server_builder` adapts both forms into the internal async route shape so dispatch stays uniform.

## Registry Concurrency Assumption

The registry is not internally synchronized.

Current code assumes:

- routes are added during setup
- request processing performs reads only
- `add(...)` does not race with `find(...)`

Because of that, concurrent route lookup is fine as long as the route table is not being mutated at the same time.

## Database Integration

`warp::db::postgres::connection_pool` uses a dedicated `boost::asio::thread_pool` for libpqxx work.

- synchronous queries package work onto the database pool and block the caller until that work completes
- asynchronous queries package work onto the database pool and resume the awaiting coroutine on the configured completion executor

The result is that a route which reaches `co_await db_pool->query(...)` does not keep an HTTP worker blocked while the database round trip is in progress.

## Practical Consequences

- slow CPU-bound work still occupies an HTTP executor thread until it completes
- coroutine handlers improve utilization when they suspend on I/O
- different client connections can make progress concurrently on different worker threads
- socket I/O is still serialized per connection because each session runs on its own strand
- callback mode is currently the lower-overhead default for raw event-loop latency
- coroutine mode remains useful when you want the server internals themselves to use coroutine control flow

For measured callback-vs-coroutine event-loop overhead, see [benchmarking.md](/Users/dnadella/Projects/warp/docs/benchmarking.md).
