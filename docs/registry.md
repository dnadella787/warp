# Warp HTTP Registry

## Purpose

`warp::http::registry` is the in-memory route table used by the HTTP server. Its job is intentionally narrow:

- store route patterns and their handlers
- match an incoming request path against those patterns
- extract path parameters from parameterized segments such as `/{id}`
- enrich the `warp::request` object with those path parameters before invoking the handler

The registry does not perform I/O, networking, request parsing, or response serialization. Those concerns live in `listener`, `http_session`, and the public request/response helpers.

## Main Types

The registry implementation lives in:

- [registry.hpp](/Users/dnadella/Projects/warp/src/http/registry.hpp)
- [registry.cpp](/Users/dnadella/Projects/warp/src/http/registry.cpp)

Important types:

- `warp::http::request`
  This is the public request wrapper over Beast. It exposes helpers such as `path()`, `query_param(...)`, `path_param(...)`, `json_body()`, and `try_json_body()`.

- `warp::http::response`
  This is the public response type returned by route handlers.

- `warp::http::handler`
  A route handler has the shape `response(const request &)`.

- `warp::http::async_handler`
  Internal dispatch uses the shape `warp::awaitable<response>(request)`.
  Public synchronous handlers are wrapped into this form so the runtime can
  execute both sync and coroutine routes through the same path.

- `warp::http::registry::segment`
  Internal representation of one compiled path segment. Each segment is either:
  - `literal`
  - `parameter`

- `warp::http::registry::route_entry`
  Internal storage for one route:
  - original route pattern string
  - compiled segment vector
  - handler

## Route Registration

Routes are registered through:

```cpp
void add(method verb, std::string path, handler h);
void add(method verb, std::string path, async_handler h);
```

When a route is added:

1. The route pattern is compiled into `segment` objects.
2. If the same pattern already exists, the existing handler is replaced.
3. Otherwise a new `route_entry` is appended to the route list.
4. Synchronous handlers are wrapped into the internal async handler form.

Example:

```cpp
registry routes;
routes.add(warp::method::get, "/users/{id}", [](const warp::request &req) {
    auto id = req.path_param("id").value_or("");
    return warp::response::ok(
        warp::body_builder().set("id", std::string(id)).build());
});
```

## Pattern Compilation

Pattern compilation happens in `compile_pattern(...)`.

Supported forms:

- `/hello`
- `/users/{id}`
- `/teams/{team_id}/members/{member_id}`
- `/`

Rules enforced during compilation:

- the pattern must start with `/`
- empty segments are rejected
  Example: `/users//42`
- parameter names must not be empty
  Example: `/users/{}` is rejected

Compilation converts a string like:

```text
/users/{id}/posts/{post_id}
```

into a segment sequence conceptually equivalent to:

```text
literal("users")
parameter("id")
literal("posts")
parameter("post_id")
```

This avoids reparsing the route pattern on every request.

## Path Matching

Incoming lookup happens through:

```cpp
std::optional<async_handler> find(method verb, std::string_view path) const;
```

The matching flow is:

1. Strip the query string from the input path.
   Example:
   `/users/42?lang=en` becomes `/users/42`

2. Split the clean path into segments.

3. Compare the request segments against each compiled route entry.

4. If all literal segments match and parameter segments line up positionally, the route matches.

5. Captured parameter values are stored in a temporary map.

6. The registry returns an adapted async handler that:
   - takes ownership of the incoming `warp::request`
   - injects the captured path parameters with `set_path_params(...)`
   - invokes the stored route handler with the enriched request

This means callers of `find(...)` receive an executable async handler directly, while path parameter extraction still works transparently.

## Example Match

Registered route:

```text
/users/{id}/posts/{post_id}
```

Incoming target:

```text
/users/42/posts/99?draft=true
```

The registry:

- strips the query string for path matching
- matches the literals `users` and `posts`
- captures:
  - `id = "42"`
  - `post_id = "99"`

The `warp::request` object passed to the user handler then exposes:

```cpp
req.path();                    // "/users/42/posts/99"
req.query_param("draft");      // "true"
req.path_param("id");          // "42"
req.path_param("post_id");     // "99"
```

## Interaction With `warp::request`

The registry only provides path parameters. Query parameters are not extracted by the registry.

Query parameters are parsed directly by `warp::request` from the Beast request target. That parsing happens when the request wrapper is constructed.

As a result:

- `query_param(...)` and `query_params()` come from the request wrapper itself
- `path_param(...)` and `path_params()` are injected by the registry when a route matches

This split keeps responsibilities clean:

- request wrapper: parse request target metadata
- registry: match routes and attach route-specific path params

## Matching Semantics

Important details of the current implementation:

- query strings are ignored for route matching
- route order matters when multiple patterns could match
  The first matching route in `routes_` wins
- segment counts must match exactly
  `/users/{id}` does not match `/users/42/posts`
- literal segments must match exactly
- parameter segments match any non-empty segment value
- the root route `/` is represented as zero segments

## Threading and Safety

The registry is currently implemented as a plain vector with no internal locking.

That is a deliberate design choice based on the current project assumption:

- routes are registered during setup
- handlers are then read during request processing
- callers do not mutate the registry concurrently with `find(...)`

Consequences:

- `find(...)` is safe for concurrent reads only if no thread is mutating `routes_`
- `add(...)` must not race with `find(...)`
- if the project later supports runtime route mutation, the registry will need synchronization or an immutable snapshot strategy

This assumption should be treated as part of the contract of the current implementation.

## Complexity

Current complexity is linear in the number of registered routes.

- `add(...)`
  - `O(n)` when checking for an existing pattern to replace
- `find(...)`
  - `O(n * s)` where `n` is route count and `s` is segment count for the candidate path

This is acceptable for a small to moderate route table, and it keeps the implementation easy to reason about.

If route counts grow substantially, likely next steps would be:

- trie-based path storage
- method-aware dispatch buckets
- immutable compiled route tables

## End-to-End Request Flow

The registry participates in the HTTP pipeline like this:

1. `listener` accepts a socket.
2. `http_session` reads an HTTP request with Beast.
3. The Beast request is wrapped as `warp::request`.
4. `registry::find(request.method(), request.target())` looks up the route.
5. If a route matches, the registry returns an adapted async handler.
6. The adapted handler injects path params into the request.
7. `http_session` starts that handler with `co_spawn(...)`.
8. The user handler eventually completes with a `warp::response`.
9. `http_session` writes that response back through Beast.

That design keeps the registry focused on route lookup while letting the public request/response API stay ergonomic.
