# Warp Threading Model

## HTTP Server Execution
- The `warp::http::server` owns a `net::core::io_context_pool` whose size is set by the `worker_threads` argument on `server_builder` (default is `max(1, std::thread::hardware_concurrency())`). One `boost::asio::io_context` instance is created per worker slot.
- Calling `server::run()` spins up one std::thread per `io_context` in the pool. Each connection handler and route callback executes on these worker threads via `io_context::run()`.
- A dedicated `boost::asio::io_context` (`accept_ctx_`) powers the single `boost::asio::ip::tcp::acceptor`. The thread that invokes `server::run()` services this accept loop by calling `accept_ctx_->run()`, so there is exactly one acceptor socket and it is driven by the caller's thread.
- Incoming sockets are accepted on the acceptor's strand and then bound to the next worker `io_context` in round-robin order, ensuring load distribution without sharing an `io_context` across threads.

## Database Integration
- `warp::db::postgres::connection_pool` integrates libpqxx with the server's executor. The pool owns a dedicated `boost::asio::thread_pool` whose size is `max(1, worker_threads)` from the constructor arguments (defaulting to hardware concurrency).
- Synchronous queries (`sync_query`) package the work and post it to the database thread pool, blocking until the packaged task completes. This keeps synchronous callers off the network threads.
- Asynchronous queries (`async_query`) also post the work to the database thread pool. Results are dispatched back through the completion executor supplied when the pool was constructed (typically one of the HTTP worker executors), so user continuations resume on the expected `io_context`.
- Each query grabs a libpqxx connection from the pool (or opens a new one up to `max_connections`), executes the SQL, and then either returns the connection to the idle deque or discards it on error, allowing database operations to proceed without blocking the HTTP threads.
