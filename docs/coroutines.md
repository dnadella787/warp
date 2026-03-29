# Warp Coroutines

## Purpose

Warp supports coroutine-based route handlers so user code can stay sequential while I/O remains non-blocking.

A route can return either:

- `warp::response`
- `warp::awaitable<warp::response>`

Warp normalizes both forms internally and always executes routes through its async dispatch path.

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
             auto result = co_await db_pool->async_query(
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

That is the main benefit of the coroutine API. Users get direct sequential control flow without manually wiring completion handlers.

## What Warp Does Internally

The internal flow for a coroutine route is:

1. `http_session` reads and parses the HTTP request.
2. The request is wrapped as `warp::request`.
3. The registry finds the matching route and returns an internal async handler.
4. `http_session` starts that handler with `boost::asio::co_spawn(...)`.
5. The route coroutine runs on the session executor until it either:
   - completes immediately, or
   - suspends at a `co_await`
6. When the coroutine completes, Warp receives the final `warp::response` in `on_handler_complete(...)`.
7. Warp stores completed responses until they are eligible to be written in request order.
8. Warp writes responses back to the socket in request order even if handlers finished out of order.

The important part is that `http_session::on_read(...)` does not block waiting for the handler to finish. It starts the coroutine and returns control to Asio.

## What `co_await` Changes

`co_await` does not make CPU work faster by itself.

What it does do:

- lets a route suspend while waiting for I/O
- frees the HTTP worker thread to do other work
- avoids blocking the session thread on database or other asynchronous operations
- keeps route logic readable and sequential

So the main performance win is throughput and thread utilization, not faster arithmetic or faster string manipulation.

## Database Example

`warp::db::postgres::connection_pool::async_query(...)` is a good example of where coroutines help.

When a route does:

```cpp
auto result = co_await db_pool->async_query(sql);
```

the behavior is:

- the HTTP route coroutine suspends
- the actual blocking libpqxx query work runs on the PostgreSQL pool's worker threads
- when the query finishes, the coroutine resumes on the expected completion executor
- the route continues and eventually `co_return`s its response

That means the HTTP worker is not blocked by the database round trip.

## How Users Get Better Performance

Coroutines help when users apply them to the right kind of work.

### Good Uses

- waiting on database queries
- waiting on other asynchronous network calls
- multi-step request flows that are mostly I/O
- validation plus I/O plus response construction in one linear function

### Poor Uses

- long CPU-heavy loops
- large synchronous file reads
- expensive JSON transformation with no suspension points
- any blocking library call that never yields control back to Asio

If a route does heavy CPU work before its first `co_await`, that work still occupies an HTTP worker thread.

## Practical Performance Guidance

### 1. Prefer async APIs inside coroutine routes

If an operation already has an awaitable form, use that form.

Good:

```cpp
auto result = co_await db_pool->async_query(sql);
```

Less good for HTTP throughput:

```cpp
auto result = db_pool->query(sql);
```

The synchronous form still works, but it blocks the calling route until the result is ready.

### 2. Keep pre-await work small

Do cheap validation and request parsing before the first suspension point, then move into async work quickly.

Good examples:

- parse `id`
- check auth headers
- validate required fields

Bad examples:

- run expensive report generation inline
- compress a large payload on the HTTP executor
- perform large blocking filesystem traversals

### 3. Use coroutines for clarity, not just syntax

Coroutines are most useful when the route would otherwise become callback-heavy.

This pattern scales well:

```cpp
auto user = co_await load_user(...);
auto permissions = co_await load_permissions(...);
co_return build_response(user, permissions);
```

Compared with nested callbacks, this is easier to maintain and less error-prone.

### 4. Move truly blocking work off the HTTP executor

Warp already does this for PostgreSQL queries.

If users introduce another blocking dependency, they should give that work its own executor or worker pool instead of calling it directly inside the route coroutine.

### 5. Understand the connection-level limit

Warp currently allows a bounded number of in-flight requests per TCP connection.

That means:

- coroutines improve utilization while waiting on I/O
- different client connections can make progress concurrently
- a single connection may have multiple handlers running concurrently
- responses on that connection are still serialized in request order

So coroutine handlers improve server throughput across many requests and many connections, while ordered writes preserve HTTP/1.1 response semantics on each socket.

## Choosing Between Sync and Coroutine Handlers

Use a synchronous handler when:

- the route is trivial
- all work is cheap and local
- no I/O suspension is needed

Use a coroutine handler when:

- the route waits on database or network I/O
- the request flow has multiple async steps
- you want linear control flow with non-blocking behavior

## Summary

Warp uses coroutines internally to keep HTTP request handling non-blocking while preserving a simple route API.

For users, the main rule is straightforward:

- if the route waits on I/O, prefer `warp::awaitable<warp::response>` and `co_await`
- if the route is cheap and local, returning `warp::response` directly is fine

That combination keeps the API simple while improving throughput under concurrent load.
