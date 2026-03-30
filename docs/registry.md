# Warp HTTP Registry

## Purpose

`warp::http::registry` is the in-memory route table used by the HTTP server.

Its job is intentionally narrow:

- store route patterns and their handlers
- bucket routes by HTTP method
- match an incoming request path against the compiled route tree
- extract path parameters such as `/{id}`
- inject those path parameters into `warp::request` before the handler runs

The registry does not perform socket I/O, request parsing, or response serialization.

## Main Types

The implementation lives in:

- [registry.hpp](/Users/dnadella/Projects/warp/src/http/router/registry.hpp)
- [registry.cpp](/Users/dnadella/Projects/warp/src/http/router/registry.cpp)

Important types:

- `warp::http::request`
  The public request type. It currently inherits Beast's string-body request and adds helpers such as `path()`, `query_param(...)`, `path_param(...)`, `json_body()`, and `try_json_body()`.

- `warp::http::response`
  The public response type returned by route handlers.

- `warp::http::handler`
  Synchronous internal registry handler shape: `response(const request &)`.

- `warp::http::async_handler`
  Internal dispatch shape: `warp::awaitable<response>(request &&)`.

- `warp::http::registry::node`
  One trie node for a specific HTTP method. A node may have:
  - literal children keyed by segment text
  - one parameter child
  - an optional terminal route entry

- `warp::http::registry::route_entry`
  The stored handler plus the compiled list of parameter slots for that route.

## Route Registration

Routes are registered through:

```cpp
void add(method verb, std::string path, handler h);
void add(method verb, std::string path, async_handler h);
```

When a route is added:

1. The path pattern is compiled into segments.
2. The registry picks the root trie for that HTTP verb.
3. Each segment descends into either:
   - a literal child, or
   - the single parameter child
4. The terminal node stores or replaces the route entry.

Because each HTTP method has its own root, paths such as `GET /name/{id}` and `DELETE /name/{id}` can coexist without conflict.

If the same method and the same structural path are added more than once, the later add replaces the handler at that terminal node.

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

`/users/{id}/posts/{post_id}` is compiled conceptually as:

```text
literal("users")
parameter("id")
literal("posts")
parameter("post_id")
```

That compilation happens once at registration time, not on every request.

## Lookup

Incoming lookup happens through:

```cpp
const async_handler *find(request &req) const;
```

The matching flow is:

1. Read the request method.
2. Select the trie root for that method.
3. Match the cleaned request path segment-by-segment.
4. Prefer a literal child when one exists for the current segment.
5. Otherwise fall back to the parameter child.
6. If the match succeeds, inject captured path params into `req`.
7. Return a pointer to the stored handler.

The registry no longer returns a separate match structure. The caller gets the final handler pointer directly.

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

- ignores `?draft=true` for path matching
- matches `users`
- captures `id = "42"`
- matches `posts`
- captures `post_id = "99"`

The resulting request exposes:

```cpp
req.path();               // "/users/42/posts/99"
req.query_param("draft"); // "true"
req.path_param("id");     // "42"
req.path_param("post_id"); // "99"
```

## Matching Semantics

Important details of the current implementation:

- query strings are ignored for route matching
- segment counts must match exactly
- literal segments must match exactly
- parameter segments match any non-empty segment
- `/` is represented as the root node with no path segments
- literal matches win over parameter matches at the same depth
- matching is method-specific because each verb has its own trie root

That last point is what enables `GET /name/{id}` and `DELETE /name/{id}` to exist at the same time.

## Interaction With `warp::request`

The request object parses request-target metadata when it is constructed from the Beast request.

That means:

- `path()` and `query_param(...)` come from `warp::request`
- `path_param(...)` is attached by the registry after a route match

The registry only deals with route-specific path parameters.

## Threading and Safety

The registry has no internal locking.

Current code assumes:

- routes are added during setup
- request processing only performs reads
- `add(...)` does not race with `find(...)`

Under that assumption, concurrent lookups are fine. Runtime route mutation would require synchronization or immutable route snapshots.

## Complexity

The current structure is method-bucketed and trie-based.

- `add(...)`
  - `O(s)`, where `s` is the number of segments in the route pattern
- `find(...)`
  - `O(s)`, where `s` is the number of path segments in the incoming request

There is no full route-table scan on every lookup anymore.

## End-to-End Request Flow

The registry participates in the HTTP pipeline like this:

1. A listener accepts a socket.
2. A session reads an HTTP request with Beast.
3. The Beast request is wrapped as `warp::request`.
4. `registry::find(req)` looks up the route and injects path params into that request.
5. The session launches the returned handler if one exists.
6. The handler eventually completes with a `warp::response`.
7. The session writes the response back through Beast.

This keeps the registry focused on route lookup while the listener and session implementations own transport behavior.
