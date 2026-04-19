# Warp HTTP Registry

## Purpose

`warp::http::registry` is the in-memory route table used by the HTTP server.

Its job is intentionally narrow:

- store route patterns and their handlers
- bucket routes by HTTP method
- match an incoming request against a compiled path trie plus query-aware route variants
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
  The stored route callback variant used by the HTTP sessions. It wraps either a sync or coroutine handler.

- `warp::http::registry::node`
  One trie node for a specific HTTP method. A node may have:
  - literal children keyed by segment text
  - one parameter child
  - zero or more terminal route variants for the same normalized path shape

- `warp::http::registry::route_entry`
  The stored handler plus:
  - compiled path parameter slots
  - query constraints for that route variant
  - optional priority
  - registration order for deterministic tie-breaking

- `warp::http::registry::query_constraint`
  One query matcher attached to a route variant. Each constraint stores:
  - the query parameter name
  - whether the parameter is `required`, `optional`, or `forbidden`
  - an optional exact string value constraint

## Route Registration

Routes are registered through:

```cpp
void add(method verb, std::string path, handler h);
void add_route(method verb, std::string path, handler h);
```

When a route is added:

1. The registration string is split into:
   - a path pattern, compiled into trie segments
   - optional query-routing constraints declared in the route string
2. The registry picks the root trie for that HTTP verb.
3. Each path segment descends into either:
   - a literal child, or
   - the single parameter child
4. The terminal node appends a new route variant to its `routes` vector.

Because each HTTP method has its own root, paths such as `GET /name/{id}` and `DELETE /name/{id}` can coexist without conflict.

The registry rejects an exact duplicate of:

- method
- normalized path shape
- normalized query-constraint set

It does not reject overlapping query variants at registration time. Ambiguity prevention for generated routes happens earlier in codegen via compile-time metadata and `static_assert`.

## Pattern Compilation

Path compilation happens in `parse_route_pattern(...)`.

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

## Query-Aware Variants

The registry supports multiple handlers for the same HTTP method and path shape by attaching query matchers to the terminal trie node.

Examples:

```text
GET /users
GET /users?summary
GET /users?summary&fields
GET /users?summary&!fields
GET /users?mode=full
```

Registration-time query syntax:

- `?name`
  required query parameter must exist
- `?~name`
  optional query parameter, used only for specificity scoring when present
- `?!name`
  query parameter must not exist
- `?name=value`
  required query parameter with an exact decoded value match
- `?priority=10`
  reserved registration-only priority override

Notes:

- priority keys are consumed by the registry and are not route constraints
- query constraint names are normalized and sorted, so registration order inside the query string does not matter
- duplicate query constraint names in a single route registration are rejected
- exact-value matching is string-based after percent-decoding
- incoming requests still reject malformed or duplicate query parameters during `warp::request` parsing

## Lookup

Incoming lookup happens through:

```cpp
const handler *find(request &req) const;
```

The matching flow is:

1. Read the request method.
2. Select the trie root for that method.
3. Match the request path segment-by-segment.
4. Prefer a literal child when one exists for the current segment.
5. Otherwise fall back to the parameter child.
6. Once the terminal node is reached, evaluate every registered route variant at that node against `req.query_param(...)`.
7. Pick the best matching variant by deterministic score ordering.
8. If a variant wins, inject captured path params into `req`.
9. Return a pointer to the stored handler.

The registry no longer returns a separate match structure. The caller gets the final handler pointer directly.

## Example Match

Registered route:

```text
/users/{id}/posts/{post_id}
```

Incoming target:

```text
/users/42/posts/99?draft=true&mode=full
```

The registry:

- uses only `/users/42/posts/99` for trie traversal
- matches `users`
- captures `id = "42"`
- matches `posts`
- captures `post_id = "99"`
- then evaluates any query-aware variants attached to that path leaf

The resulting request exposes:

```cpp
req.path();               // "/users/42/posts/99"
req.query_param("draft"); // "true"
req.query_param("mode");  // "full"
req.path_param("id");     // "42"
req.path_param("post_id"); // "99"
```

## Matching Semantics

Important details of the current implementation:

- trie traversal is still path-only
- segment counts must match exactly
- literal segments must match exactly
- parameter segments match any non-empty segment
- `/` is represented as the root node with no path segments
- literal matches win over parameter matches at the same depth
- matching is method-specific because each verb has its own trie root
- query constraints are evaluated only after the path leaf is reached

If multiple route variants match the same request at a leaf, the winner is:

1. the variant with the most matched constraints
2. then the variant with the most matched exact-value constraints
3. then the variant with the highest explicit priority
4. then the earliest registration order

This means unconstrained routes naturally act as fallbacks because they score lower than matching constrained variants.

That last point is what enables `GET /name/{id}` and `DELETE /name/{id}` to exist at the same time.

## Interaction With `warp::request`

The request object parses request-target metadata when it is constructed from the Beast request.

That means:

- `path()` and `query_param(...)` come from `warp::request`
- `path_param(...)` is attached by the registry after a route match

The registry relies on `warp::request` target parsing for:

- decoded query parameters
- duplicate query parameter rejection
- malformed percent-encoding rejection

When target parsing fails, `find(...)` can still match a fallback route, but query-constrained variants usually stop matching because the parsed query map is empty. Later request binding still sees the target error through `req.target_error()`.

## Threading and Safety

The registry has no internal locking.

Current code assumes:

- routes are added during setup
- request processing only performs reads
- `add(...)` does not race with `find(...)`

Under that assumption, concurrent lookups are fine. Runtime route mutation would require synchronization or immutable route snapshots.

## Complexity

The current structure is method-bucketed, trie-based, and variant-aware at each terminal node.

- `add(...)`
  - `O(s + q log q + v)` where:
  - `s` is the number of path segments
  - `q` is the number of query constraints in the registration string
  - `v` is the number of existing variants already attached to that path leaf for duplicate checking
- `find(...)`
  - `O(s + v * q)` in the current implementation where:
  - `s` is the number of path segments in the incoming request
  - `v` is the number of route variants at the matched path leaf
  - `q` is the number of constraints per variant

There is still no full route-table scan on every lookup. Only the variants that share the matched method and path leaf are inspected.

## End-to-End Request Flow

The registry participates in the HTTP pipeline like this:

1. A listener accepts a socket.
2. A session reads an HTTP request with Beast.
3. The Beast request is wrapped as `warp::request`.
4. `registry::find(req)` looks up the route and injects path params into that request.
5. The session launches the returned handler if one exists.
6. The handler eventually completes with a `warp::response`.
7. The session writes the response back through Beast.

This keeps the registry focused on deterministic route lookup while the listener and session implementations own transport behavior.
