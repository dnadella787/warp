# Warp Threading Model

## HTTP Server Execution
- The current server implementation owns a single `boost::asio::io_context` in `server::impl`.
- `worker_threads(count)` controls how many threads call `io_context::run()` on that single context. The effective pool size is `max(1, count)`.
- `server::run(bool blocking)` starts those worker threads and also starts the listener on the same `io_context`.
- If `blocking == true`, the calling thread also enters `io_context::run()`. In that mode, work may execute on:
  - each spawned worker thread
  - the caller's thread
- If `blocking == false`, only the spawned worker threads service the `io_context`.

## Accept Loop
- The listener owns exactly one `boost::asio::ip::tcp::acceptor`.
- The acceptor is constructed on `boost::asio::make_strand(io_context)`, so accept operations are serialized through that strand even though multiple threads may be running the same `io_context`.
- `listener::run()` dispatches into the acceptor strand and begins the asynchronous accept loop.
- Each accepted socket is rebound onto its own strand before the session starts. This gives each `http_session` serialized access to its own socket operations.

## Session Execution
- Each `http_session` performs asynchronous reads and writes on a `boost::beast::tcp_stream`.
- The session stores outgoing responses in an internal queue and writes them asynchronously on the shared server `io_context`.
- Request parsing and response writing are both asynchronous and execute on the shared server `io_context`.
- Route handlers are normalized to an internal async form and started with `boost::asio::co_spawn(...)` from `http_session::on_read(...)`.
- Coroutine handlers run on the session executor until they suspend at a `co_await` or complete.
- Warp currently keeps request processing sequential per connection. The next `do_read()` on a socket does not start until the current response has been fully written.

## Request and Route Handling
- Incoming Beast requests are wrapped as `warp::request` before dispatch.
- `warp::request` parses request-target metadata locally, including:
  - clean path
  - query parameters
  - JSON body helpers
- The HTTP registry matches only against the request path portion of the target.
- If a route pattern includes parameters such as `/{id}`, the registry captures those values and injects them into the `warp::request` object before calling the user handler.
- Public route handlers may return either `warp::response` or `warp::awaitable<warp::response>`.
- Synchronous handlers are wrapped into the internal async route form so the dispatch path stays consistent.

## Registry Threading Assumption
- The registry is no longer internally synchronized.
- Current code assumes routes are added during setup and not mutated while request handling is in progress.
- Because of that assumption:
  - concurrent `find(...)` calls are fine as long as no thread is mutating the route table
  - `add(...)` must not race with `find(...)`
- If runtime route mutation is introduced later, the registry will need synchronization or immutable snapshots.

## Database Integration
- `warp::db::postgres::connection_pool` integrates libpqxx with the server's executor. The pool owns a dedicated `boost::asio::thread_pool` whose size is `max(1, worker_threads)` from the constructor arguments (defaulting to hardware concurrency).
- Synchronous queries (`sync_query`) package the work and post it to the database thread pool, blocking until the packaged task completes. This keeps synchronous callers off the network threads.
- Asynchronous queries (`async_query`) also post the work to the database thread pool. Results are dispatched back through the completion executor supplied when the pool was constructed (typically one of the HTTP worker executors), so coroutine continuations resume on the expected `io_context`.
- Each query grabs a libpqxx connection from the pool (or opens a new one up to `max_connections`), executes the SQL, and then either returns the connection to the idle deque or discards it on error, allowing database operations to proceed without blocking the HTTP threads.

## Practical Consequences
- A slow route handler will occupy one of the HTTP `io_context` threads until it either completes or suspends.
- A route that quickly reaches `co_await db_pool.async_query(...)` frees that HTTP worker while the database query runs on the PostgreSQL pool.
- Blocking work should be moved off the HTTP path, for example into the PostgreSQL pool or another executor.
- Socket I/O remains serialized per connection because each session uses its own strand, but different sessions may progress concurrently on different threads servicing the shared `io_context`.
- Coroutines improve throughput by avoiding blocked HTTP workers during I/O waits, but they do not make CPU-bound work faster by themselves.
