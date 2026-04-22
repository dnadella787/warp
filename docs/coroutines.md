# Warp Coroutines

## Purpose

Warp supports coroutine-based route handlers so user code can stay sequential while I/O remains non-blocking.

A route can return either:

- `warp::response`
- `warp::awaitable<warp::response>`

`server_builder` normalizes both forms into the internal async shape `warp::awaitable<warp::response>(warp::request&&)`, so the server always dispatches routes through one async path.

## User-Facing Model

Users do not need to call `boost::asio::co_spawn` to use coroutines in route handlers.

This is valid Warp code:

```cpp
.get("/db/{id}",
     [db_pool](warp::request req) -> warp::awaitable<warp::response> {
         auto id = req.path_param("id").value_or("");
         if (!is_integer(id)) {
             co_return warp::response::bad_request("id must be an integer");
         }

         try {
             auto result = co_await db_pool->query(
                 std::string("select ") + std::string(id) +
                 "::int as requested_id, current_database() as database_name");

             co_return warp::response::ok(
                 warp::body_builder()
                     .set("requested_id", result.rows() > 0 ? std::string(result.value(0, 0)) : std::string(id))
                     .set("database_name",
                          result.rows() > 0 ? std::string(result.value(0, 1)) : std::string {})
                     .build());
         } catch (const std::exception &ex) {
             co_return warp::response::server_error(ex.what());
         }
     })
```

The code reads top-to-bottom:

1. inspect the request
2. validate input
3. `co_await` async work
4. `co_return` a response

## Event Loop Modes

Warp now has two internal server event-loop implementations:

- `warp::event_loop_mode::callbacks`
- `warp::event_loop_mode::coroutines`

The default is `callbacks`.

Users can switch modes through the server builder:

```cpp
auto server = warp::http::server_builder()
    .event_loop(warp::event_loop_mode::coroutines)
    .get("/ping", [](warp::request) {
        return warp::response::ok("{\"status\":\"ok\"}");
    })
    .build();
```

This setting changes how Warp runs the accept loop and HTTP session internals. It does not change the route API. Coroutine route handlers work in both modes.

## Internal Flow

The shared dispatch flow is:

1. A listener accepts a socket.
2. A session reads and parses an HTTP request.
3. The Beast request is wrapped as `warp::request`.
4. The registry matches the route and injects path params into the request.
5. Warp starts the normalized async handler.
6. The handler either completes immediately or suspends at a `co_await`.
7. When the handler completes, Warp stores the response until it can be written in request order.
8. The session writes responses back in HTTP/1.1 request order.

The difference between the two modes is how steps 1, 2, 5, and 8 are driven internally.

### Callback Mode

Callback mode uses:

- [callback_listener.cpp](../src/http/listener/callback_listener.cpp)
- [callback_http_session.cpp](../src/http/session/callback_http_session.cpp)

Flow:

1. `listener::run()` dispatches onto the acceptor strand.
2. `listener::do_accept()` starts `async_accept(...)`.
3. `http_session::on_read(...)` receives each parsed request.
4. The route handler is launched with `boost::asio::co_spawn(...)`.
5. `on_handler_complete(...)` stores the finished response.
6. `async_write(...)` flushes ready responses in sequence order.

`on_read(...)` does not block waiting for the handler to finish. It launches the coroutine and returns to Asio.

### Coroutine Mode

Coroutine mode uses:

- [coroutine_listener.cpp](../src/http/listener/coroutine_listener.cpp)
- [coroutine_http_session.cpp](../src/http/session/coroutine_http_session.cpp)

Flow:

1. `coroutine_listener::run()` starts the accept loop with `boost::asio::co_spawn(...)`.
2. `accept_loop()` repeatedly `co_await`s `async_accept(...)`.
3. Each `coroutine_http_session` starts a coroutine `read_loop()` and `write_loop()`.
4. `read_loop()` parses requests and launches each matched route handler with `co_spawn(...)`.
5. `write_loop()` waits until the next response in sequence is ready, then `co_await`s `async_write(...)`.
6. Internal timers are used as wake signals so the read loop can pause at the pipeline limit and the write loop can sleep until a response becomes ready.

This path is more coroutine-native internally, but user route code stays the same.

## What `co_await` Changes

`co_await` does not make CPU work faster by itself.

What it does do:

- lets a route suspend while waiting for I/O
- frees the HTTP worker thread to do other work
- avoids blocking the session executor on database or other asynchronous operations
- keeps route logic readable and sequential

The main gain is better thread utilization and throughput under I/O-heavy load.

## Database Example

`warp::db::postgres::connection_pool::query(...)` is a good fit for coroutine routes.

When a route does:

```cpp
auto result = co_await db_pool->query(sql);
```

the behavior is:

- the HTTP route coroutine suspends
- the blocking libpqxx work runs on the PostgreSQL pool threads
- when the query finishes, the coroutine resumes on the expected completion executor
- the route continues and eventually `co_return`s its response

That means the HTTP worker is not blocked by the database round trip.

## Performance Guidance

Coroutines help most when users apply them to I/O-bound work.

Good uses:

- database queries
- outbound HTTP or RPC calls
- multi-step request flows with several async waits

Poor uses:

- long CPU-heavy loops
- large blocking file reads
- expensive transformations with no suspension points
- libraries that block the calling thread and never yield to Asio

Practical rules:

1. Prefer async APIs inside coroutine routes.
2. Keep work before the first `co_await` small.
3. Move truly blocking work onto another executor or thread pool.
4. Expect ordered writes per connection even when handlers complete out of order.

## Connection-Level Behavior

Warp allows a bounded number of in-flight requests per TCP connection.

That means:

- one connection may have several route handlers running concurrently
- handlers may complete out of order
- responses are still written strictly in request order
- if the request path eventually closes the connection, Warp stops reading new requests and drains what is already accepted

## Benchmarking

The callback event loop is still the default because it currently has the lower event-loop overhead on Warp's local round-trip benchmark. The latest measurements are documented in [benchmarking.md](benchmarking.md).

## Summary

For users, the rule is simple:

- if the route waits on I/O, prefer `warp::awaitable<warp::response>` and `co_await`
- if the route is trivial and local, returning `warp::response` directly is fine

The event-loop mode only changes Warp's internal execution model. The public coroutine route API stays the same.
