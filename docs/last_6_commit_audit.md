# Audit Scope

- Repo: `warp`
- Branch: `main`
- Reviewed range: `HEAD~6..HEAD` ending at `c282a41090c7e6e9cfff07c53710ce19c4f20455`
- Commits reviewed:
  - `c282a41` Cleanup
  - `0e373f8` Cleanup route_constraints
  - `4990817` Update generated class to support builders + setter/getter
  - `8a5741a` alias collision and other fixes
  - `f11b35f` Update registry to search by query params as well
  - `c3f9b4b` Update registry to use std::ranges
- Primary areas changed:
  - query-aware registry lookup and route registration
  - compile-time route metadata (`route_path`, `route_spec`, query constraints)
  - generated request/response adapters and overload selection
  - generated data class surface
- Validation performed:
  - static audit of the diff and affected code paths
  - targeted `ctest --test-dir build -R 'RequestTest|RegistryTest|ApiModelTest|ResourceEmitterTest|GeneratedApiIntegrationTest|GeneratedQueryRoutingIntegrationTest' --output-on-failure`
  - 52 tests passed; several socket-based integration tests were skipped in this environment

# Current Status Update

This document began as a point-in-time audit of the last six commits in the reviewed range above. The repository has
changed materially since then. Another agent reading this file should treat the findings below as historical context,
not as the current source of truth for what still needs to be fixed.

Current status in the repo/worktree as of 2026-04-21:

Resolved since the original audit:

- literal-first trie fallback bug is fixed; the router now falls back from a failing literal branch to a parameter
  sibling, and there is a regression test covering it
- request query/path lookup now supports true heterogeneous `std::string_view` lookup through transparent maps
- singleton required-query generated endpoints now stay unconstrained, preserving binder-driven `400` responses, and
  there is now a dedicated integration test covering this behavior
- compile coverage for `noexcept` generated handlers exists
- typed route registration no longer round-trips through `route_spec -> string -> runtime parse`; the repo now has a
  real `compiled_route` type, `compile_route_spec(...)`, and `registry.add_compiled(...)`
- public headers no longer depend directly on `src/` router internals; shared route helpers now live under
  `include/warp/http/...`
- generated sync handlers no longer always pay the async-wrapper path; sync generated handlers register as sync
  handlers when possible
- generated request parsing now uses unchecked inner path/query helpers after the outer target-error guard, avoiding
  redundant target-error checks per binding
- response-side codegen no longer uses fragile getter-member-pointer `static_cast` contracts; generated response traits
  call accessors directly

Still open and still worth work:

- request-path traversal still allocates a `std::vector<std::string_view>` via `split_route_path_views(...)`; the
  original "split twice" finding is no longer current, but a lower-allocation offset/view traversal is still an
  available hot-path improvement
- same-leaf query variant resolution is still a linear `O(v * q)` scan; no indexed/query-mask dispatch exists yet
- route semantics are still duplicated across three layers:
  compile-time ambiguity logic in `route_spec.hpp`, codegen query-route grouping in `src/codegen/model.cpp`, and
  runtime route matching/scoring in `registry.cpp`
- the compile-time exact-value ambiguity checker still appears to be a separate algorithm from runtime winner
  selection; existing compile coverage is better, but the deeper semantic unification has not happened
- generated builder/accessor surface area remains large; the build-time/code-size recommendation to shrink or make that
  surface optional is still open
- trie/node storage is still pointer-heavy (`unordered_map` + `unique_ptr` per node); the arena/flat-layout redesign
  remains open
- codegen still emits one route/binding per endpoint rather than a deeper group-level dispatcher for overlapping
  same-path query groups

Recommended remaining work:

1. If request-path performance matters, replace `split_route_path_views(...)` with a lower-allocation path traversal
   that carries captures directly into path-param extraction.
2. If same-path query route groups are growing, replace leaf-local linear scans with an indexed/query-mask decision
   path.
3. If more query-routing features are planned, unify route semantics further around the existing `compiled_route`
   foundation instead of keeping compile-time, codegen, and runtime logic partially separate.
4. Revisit the compile-time exact-value ambiguity algorithm only if deterministic exact-value route sets continue to be
   rejected incorrectly in practice.
5. Tackle generated-code build-time/code-size and trie-layout work only after correctness/semantic-unification needs
   are stable.

# Historical Executive Summary

- Overall risk level: High
- The biggest correctness issue is in the router, not codegen: the trie still commits to a literal branch before query scoring, so a literal leaf whose query constraints fail can hide a parameter sibling that should have matched.
- The biggest request-path performance regression is the new query-aware routing path repeatedly materializing temporary `std::string` keys and repeatedly splitting request paths into owned `std::string` segments.
- The codegen changes improved expressiveness, but they now duplicate routing semantics across three layers: compile-time `route_spec`, codegen route grouping, and runtime registry parsing/scoring.
- The generated adapter path adds notable compile-time/code-size overhead and still forces synchronous user handlers onto the async dispatch path.
- Two trivial micro-fixes were applied directly:
  - additive `request::query_param(const std::string&)` / `path_param(const std::string&)` overloads to avoid temporary-key construction when callers already hold `std::string`
  - removal of a redundant per-request `shared_ptr` capture inside `warp::codegen::bind_endpoint`

Top 5 findings:

1. Literal-first trie traversal can return `nullptr` even when a parameter sibling should match.
2. Query-aware routing pays avoidable per-constraint temporary key construction on the hot path.
3. Successful parameterized matches split and allocate the path twice.
4. Codegen likely changes singleton required-query endpoints from `400` binding failures to `404` misses.
5. Same-path query variants are resolved by an `O(v*q)` leaf scan, which will scale poorly as generated route groups grow.

Biggest performance concerns:

1. request-path: repeated `std::string` key materialization in request query/path lookup
2. request-path: repeated owned path splitting in [route_pattern.hpp](../src/http/router/route_pattern.hpp:258)
3. request-path: leaf-local linear scans of query variants in [registry.cpp](../src/http/router/registry.cpp:270)
4. startup-path: `route_spec` metadata serialized back into a string and reparsed into runtime structures
5. build-time: large emitted class/adapter surface and overload probing across many member-pointer signatures

Biggest correctness concerns:

1. literal/query route can shadow valid parameter fallback
2. singleton required-query generated routes likely skip binder validation and return `404`
3. compile-time route ambiguity checker over-rejects some deterministic exact-value route sets

# Findings by Area

## Routing / Route Registry

### Literal-First Trie Walk Can Drop Valid Parameter Matches
Severity: Critical  
Confidence: High  
Where: [registry.cpp](../src/http/router/registry.cpp:235), [registry.cpp](../src/http/router/registry.cpp:260)  
Why it matters: a request can match a literal child structurally, fail its query constraints at the leaf, and never fall back to a parameter sibling that should have matched. That is a real functional regression in the new query-aware lookup path.  
Evidence from the code/commits: `f11b35f` changed lookup to score query variants only after the path leaf is reached, but traversal still greedily prefers the literal child over the parameter child. Once [registry.cpp](../src/http/router/registry.cpp:260) takes the literal edge, the parameter branch is no longer explored.  
Likely impact: incorrect `404` or wrong route selection for route sets such as `GET /users/me?summary` plus `GET /users/{id}` when the request is `/users/me`.  
Recommendation: change lookup to collect candidates from both literal and parameter branches when both are viable, then apply the existing specificity scoring across the full candidate set.  
Estimated implementation effort: M  
Disposition: Recommendation only

### Query-Aware Dispatch Materializes Temporary Keys Per Constraint
Severity: High  
Confidence: High  
Where: [request.hpp](../include/warp/http/request.hpp:102), [registry.cpp](../src/http/router/registry.cpp:285)  
Why it matters: the new query-aware registry path performs one request query lookup per constraint per candidate route. Before the direct fix, that path rebuilt a key object for each lookup.  
Evidence from the code/commits: `f11b35f` introduced [registry.cpp](../src/http/router/registry.cpp:285) `match_query_constraints`, which repeatedly calls `req.query_param(constraint.name)`. The original implementation of [request.hpp](../include/warp/http/request.hpp:102) constructed `std::string(key)` before `unordered_map::find`.  
Likely impact: avoidable CPU and allocator churn on every query-constrained request, multiplied by `variants * constraints`.  
Recommendation: complete the cleanup by moving request query/path maps to heterogeneous `string_view` lookup instead of relying on overload selection alone.  
Estimated implementation effort: S  
Disposition: Partially fixed directly

### Successful Parameterized Matches Split the Path Twice
Severity: High  
Confidence: High  
Where: [registry.cpp](../src/http/router/registry.cpp:235), [registry.cpp](../src/http/router/registry.cpp:331), [route_pattern.hpp](../src/http/router/route_pattern.hpp:258)  
Why it matters: the request path is tokenized once to traverse the trie and again to extract path params after a match. Both passes allocate `std::vector<std::string>` and copy segments.  
Evidence from the code/commits: `match_route` calls `split_route_path(req.path())`; successful matches then call `apply_path_params`, which calls `split_route_path(path)` again. `c3f9b4b` retained the owned-string path splitter.  
Likely impact: measurable latency and allocation overhead on every successful parameterized route, exactly where the new query-route tests spend their time.  
Recommendation: traverse using `std::string_view` segments or offsets, and carry the matched capture positions through to path-param extraction so the path is split once.  
Estimated implementation effort: M  
Disposition: Recommendation only

### Same-Leaf Variant Resolution Is `O(v*q)` and Bloats Node Footprint
Severity: Medium  
Confidence: High  
Where: [registry.hpp](../src/http/router/registry.hpp:66), [registry.hpp](../src/http/router/registry.hpp:78), [registry.cpp](../src/http/router/registry.cpp:270)  
Why it matters: the new registry no longer has one terminal route per path shape. Every matched leaf now scans every variant and checks every constraint. Each node also permanently carries a `std::vector<route_entry>`, even when it is not terminal.  
Evidence from the code/commits: `f11b35f` replaced the single terminal route with `std::vector<route_entry> routes` and linear best-match selection after traversal. The project docs now describe lookup as `O(s + v * q)`.  
Likely impact: acceptable for a few variants, but poor for heavily overloaded method/path groups and less cache-dense than necessary.  
Recommendation: store leaf variant buckets separately from non-terminal nodes and pre-index variants by required keys and exact-value constraints.  
Estimated implementation effort: M-L  
Disposition: Recommendation only

## Codegen

### Singleton Required-Query Endpoints Likely Regress From `400` to `404`
Severity: High  
Confidence: High, inferred  
Where: [model.cpp](../src/codegen/model.cpp:426), [resource_stub_emitter.cpp](../src/codegen/resource_stub_emitter.cpp:167), [server_builder.hpp](../include/warp/http/server_builder.hpp:171), [callback_http_session.cpp](../src/http/session/callback_http_session.cpp:101)  
Why it matters: generated endpoints with a required query parameter are now emitted as query-constrained routes even when they are the only endpoint for that path. Missing required queries then fail route matching before request binding can produce a `400`.  
Evidence from the code/commits: `build_query_route_model` returns a query route whenever a request has required query parameters. `emit_query_route_spec_aliases` emits query-constrained `route_spec` aliases for every such endpoint, not only for overlapping groups. Session code returns `response::not_found()` when no route matches.  
Likely impact: externally visible behavior change for singleton endpoints such as `GET /reports/{id}?summary`; missing `summary` likely becomes `404 Not Found` instead of `400 missing_query_parameter`.  
Recommendation: only generate query-constrained route specs for groups that actually have competing same-path endpoints. Keep singleton endpoints on the plain path and let request binding enforce required query parameters.  
Estimated implementation effort: S-M  
Disposition: Recommendation only

### Overload Resolution Rejects Valid `noexcept` Service Handlers
Severity: Medium  
Confidence: High, inferred  
Where: [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:216), [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:595), [resource_stub_emitter.cpp](../src/codegen/resource_stub_emitter.cpp:299)  
Why it matters: `noexcept` member functions are a valid handler shape, but the generated selector only probes non-`noexcept` member-pointer types.  
Evidence from the code/commits: `8a5741a` introduced overload resolution by enumerating exact member-pointer signatures and checking them with `static_cast<Signature>(&Service::handler)`. That signature list omits `noexcept` forms.  
Likely impact: compile-time regression for otherwise valid services, with no current compile test covering it.  
Recommendation: either include `noexcept` variants or replace exact pointer-type enumeration with invocation-based `requires` checks that let normal overload resolution handle qualifiers.  
Estimated implementation effort: M  
Disposition: Recommendation only

### Generated Builder/Accessor Surface Increased Header Size and Parse Indirection
Severity: Medium  
Confidence: High  
Where: [data_class_emitter.cpp](../src/codegen/data_class_emitter.cpp:103), [resource_stub_emitter.cpp](../src/codegen/resource_stub_emitter.cpp:218), [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:465)  
Why it matters: every generated schema/request/response now emits builder APIs, setter/getter triplets, and per-field accessor helpers, even for internal request envelopes that are only used by generated adapters.  
Evidence from the code/commits: `4990817` expanded the generated class surface materially; request parsing now routes values through `Binding::set -> Accessor::set -> set_field` instead of assigning storage directly. The example generated types file grew substantially in the reviewed range.  
Likely impact: more compile time, larger debug info and object text, and extra value movement for large parsed bodies without much runtime benefit on server hot paths.  
Recommendation: make the fluent surface optional, or stop generating it for internal request envelopes. If the current surface stays, allow forwarding/`&&` setter paths for internal adapter use.  
Estimated implementation effort: M-L  
Disposition: Recommendation only

### Generated Request Contracts Re-Check Target Errors for Every Binding
Severity: Low  
Confidence: High  
Where: [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:472), [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:299), [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:312)  
Why it matters: this is repeated work on every generated request parse.  
Evidence from the code/commits: `generated_request_contract::parse` exits early on `request_target_binding_error(req)`, but the path/query helpers invoked by each binding perform the same check again.  
Likely impact: small but pervasive extra branching on the request hot path.  
Recommendation: keep a single outer target-error guard for generated contracts and use unchecked path/query helpers internally.  
Estimated implementation effort: S  
Disposition: Recommendation only

## Async / I/O Flow

### Generated Sync Handlers Still Pay Async Wrapper Overhead
Severity: Medium  
Confidence: High  
Where: [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:629), [server_builder.hpp](../include/warp/http/server_builder.hpp:199), [callback_http_session.cpp](../src/http/session/callback_http_session.cpp:94)  
Why it matters: even synchronous service methods now go through an `awaitable<response>` wrapper and are dispatched as `async_handler`, but they still run on the socket executor. That means they keep the same blocking risk while adding coroutine overhead.  
Evidence from the code/commits: `bind_endpoint` always returns `warp::awaitable<warp::response>`, `server_builder::make_route_handler` stores that as `async_handler`, and both session implementations `co_spawn` async handlers.  
Likely impact: extra coroutine frame/scheduling overhead for generated sync endpoints with no reduction in strand blocking.  
Recommendation: emit separate sync and async binders, registering sync overloads as `sync_handler`.  
Estimated implementation effort: M  
Disposition: Recommendation only

### Redundant Inner `shared_ptr` Capture in `bind_endpoint`
Severity: Low  
Confidence: High  
Where: [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:627)  
Why it matters: the outer generated coroutine already owns the service `shared_ptr`; capturing it again in the inner lambda adds a refcount operation and enlarges the closure.  
Evidence from the code/commits: this pattern was introduced in the adapter-based path from `8a5741a`/`4990817`.  
Likely impact: small per-request overhead and larger coroutine state.  
Recommendation: keep ownership in the outer coroutine and capture `Service*` in the inner lambda.  
Estimated implementation effort: XS  
Disposition: Fixed directly

## Memory / Allocations

### `route_spec` Data Round-Trips Through a Runtime String
Severity: Medium  
Confidence: High  
Where: [server_builder.hpp](../include/warp/http/server_builder.hpp:171), [server_builder.cpp](../src/http/server_builder.cpp:53), [registry.cpp](../src/http/router/registry.cpp:161)  
Why it matters: compile-time route metadata is flattened into a string at registration time and then reparsed into owned runtime structures. That duplicates work and risks semantic drift.  
Evidence from the code/commits: `0e373f8` introduced `route_spec`; `server_builder::registered_path()` serializes its query constraints into a string; `build()` then feeds that string back through `registry.add`, which reparses it via `parse_registered_route`.  
Likely impact: startup allocation and parsing overhead for every generated route, plus long-term maintenance risk because compile-time and runtime parsers must stay identical.  
Recommendation: add a direct registration path that consumes a compiled route descriptor instead of re-serializing through a query string.  
Estimated implementation effort: M  
Disposition: Recommendation only

### Trie Layout Remains Pointer-Heavy and Cache-Unfriendly
Severity: Low  
Confidence: Medium  
Where: [registry.hpp](../src/http/router/registry.hpp:66), [registry.cpp](../src/http/router/registry.cpp:77)  
Why it matters: each literal edge is still an `unordered_map` lookup followed by a `unique_ptr` hop, which is not a good layout for large route tables.  
Evidence from the code/commits: the reviewed range did not redesign the trie; it extended it with query-aware leaves.  
Likely impact: poorer locality as the route set grows, especially compared with arena-backed or flatter child representations.  
Recommendation: for a deeper redesign, move node ownership to contiguous storage and replace per-node `unordered_map` with a flatter edge representation.  
Estimated implementation effort: L  
Disposition: Recommendation only

## Correctness / Edge Cases

### Compile-Time Ambiguity Checker Over-Rejects Some Deterministic Exact-Value Sets
Severity: Medium  
Confidence: High, inferred  
Where: [route_spec.hpp](../include/warp/http/route_spec.hpp:85)  
Why it matters: the compile-time ambiguity checker intersects score ranges independently instead of modeling feasible `(matched_constraints, matched_exact_constraints)` pairs jointly.  
Evidence from the code/commits: `0e373f8` introduced `route_spec` ambiguity logic, but runtime winner selection in [registry.cpp](../src/http/router/registry.cpp:317) depends on the joint score tuple.  
Likely impact: valid exact-value route sets can be rejected at compile time even though runtime scoring would never tie.  
Recommendation: compute reachable score tuples, not independent per-dimension overlap tests, and add compile tests covering exact-value constraints.  
Estimated implementation effort: M  
Disposition: Recommendation only

### Compile-Time Query Serialization Likely Mis-Encodes Special Characters
Severity: Medium  
Confidence: Medium, inferred  
Where: [server_builder.hpp](../include/warp/http/server_builder.hpp:175), [registry.cpp](../src/http/router/registry.cpp:176)  
Why it matters: compile-time query names and exact values are appended into the registered path without URL encoding, but runtime reparses them as a query string and decodes `+`, `%`, `&`, and `=` specially.  
Evidence from the code/commits: `registered_path()` appends raw `constraint.name` / `constraint.exact_value`; `parse_registered_route` then decodes them as URL query components.  
Likely impact: wrong registration or runtime exceptions for constraints containing reserved query characters.  
Recommendation: either percent-encode names/values during serialization or eliminate string serialization from this path entirely.  
Estimated implementation effort: S-M  
Disposition: Recommendation only

## Maintainability / Refactor Opportunities

### Routing Semantics Are Duplicated Across Three Layers
Severity: Medium  
Confidence: High  
Where: [route_spec.hpp](../include/warp/http/route_spec.hpp:85), [model.cpp](../src/codegen/model.cpp:448), [registry.cpp](../src/http/router/registry.cpp:317)  
Why it matters: compile-time route determinism, codegen route grouping, and runtime route scoring are maintained separately and are already drifting at the edges.  
Evidence from the code/commits: the compile-time layer has its own ambiguity math, codegen groups routes using
accepted/required parameter sets only, and runtime supports additional features such as exact-value constraints.  
Likely impact: more latent correctness gaps as query-routing features expand.  
Recommendation: unify around a shared compiled route descriptor and shared specificity semantics consumed by codegen, `server_builder`, and the runtime registry.  
Estimated implementation effort: L  
Disposition: Recommendation only

### Public Headers Depend on `src/` Internals
Severity: Low  
Confidence: High  
Where: [request.hpp](../include/warp/http/request.hpp:15), [route_spec.hpp](../include/warp/http/route_spec.hpp:8)  
Why it matters: public interfaces now include `src/http/router/route_pattern.hpp`, which couples installable headers to private source layout.  
Evidence from the code/commits: both files include the private router header directly.  
Likely impact: packaging fragility and harder refactoring of internal router code.  
Recommendation: move the shared route parsing/validation declarations into a public internal header under `include/warp/http` or split the public pieces out cleanly.  
Estimated implementation effort: S-M  
Disposition: Recommendation only

# Performance Bottleneck Review

Likely hot paths:

1. `registry::find -> match_route -> match_query_constraints`
2. `registry::apply_path_params`
3. generated request binding in `generated_request_contract::parse`
4. generated endpoint dispatch in `bind_endpoint`

Suspected bottlenecks:

1. Temporary key materialization for query/path lookup  
Path type: request-path  
Evidence: [request.hpp](../include/warp/http/request.hpp:102) and [request.hpp](../include/warp/http/request.hpp:117)

2. Owned-string path splitting, often twice per successful request  
Path type: request-path  
Evidence: [route_pattern.hpp](../src/http/router/route_pattern.hpp:258), [registry.cpp](../src/http/router/registry.cpp:235), [registry.cpp](../src/http/router/registry.cpp:331)

3. Linear same-leaf query-variant scan  
Path type: request-path  
Evidence: [registry.cpp](../src/http/router/registry.cpp:270)

4. `route_spec` -> string -> runtime parse round-trip  
Path type: startup-path  
Evidence: [server_builder.hpp](../include/warp/http/server_builder.hpp:171), [server_builder.cpp](../src/http/server_builder.cpp:53)

5. Generated builders/accessors and overload probes  
Path type: build-time  
Evidence: [data_class_emitter.cpp](../src/codegen/data_class_emitter.cpp:103), [http_adapter.hpp](../include/warp/codegen/http_adapter.hpp:191)

Ranked recommendations by expected payoff:

1. Fix the literal/parameter search bug and redesign candidate collection so query scoring sees all reachable routes.
2. Replace owned-string request-path splitting with view-based traversal and reuse capture positions for path params.
3. Move request param lookup to true heterogeneous `string_view` lookup.
4. Replace leaf-local linear scans with indexed/query-mask based resolution for same-path route groups.
5. Remove compile-time/runtime string round-tripping by registering a compiled route descriptor directly.

# Deep Refactor Opportunities

## Shared Compiled Route Descriptor Pipeline

Why it may outperform the current structure: one source of truth for normalized path segments, query predicates, and
specificity metadata eliminates duplicate parsing and semantic drift.  
Migration complexity: M-L  
Risks / tradeoffs: touches `route_spec`, `server_builder`, codegen, and registry registration together.  
Prototype or measure first: add an internal `compiled_route` consumed only by generated resources and compare startup allocations and server build time.

## Group-Level Generated Dispatchers

Why it may outperform the current structure: codegen already computes `route_group_model`, but still emits one registry entry per endpoint. A group dispatcher could register once per method/path group, reduce wrapper count, and make same-path routing decisions without a general-purpose leaf scan.  
Migration complexity: M-L  
Risks / tradeoffs: denser generated code and possible build-time blow-up if emitted naively.  
Prototype or measure first: collapse only overlapping same-path query groups and compare registry entry count, binary size, and steady-state latency.

## Leaf-Local Query Decision Tables

Why it may outperform the current structure: for generated groups, the relevant query-name universe is already known. Presence/value masks or small decision tables can turn leaf resolution from linear scans into a few lookups.  
Migration complexity: M  
Risks / tradeoffs: malformed-query and fallback semantics need careful preservation.  
Prototype or measure first: start with presence-only masks for generated groups, leave exact-value routes on the current path, and benchmark 1/4/8/16 overlapping variants.

## Arena-Backed Trie Storage

Why it may outperform the current structure: current nodes rely on `unordered_map` plus pointer chasing. Arena-backed indices and flatter edge tables should improve locality and simplify sharing of capture metadata.  
Migration complexity: L  
Risks / tradeoffs: more intrusive rewrite, lower immediate payoff than fixing candidate selection and leaf resolution first.  
Prototype or measure first: keep the current matching logic but replace `unique_ptr` ownership with indexed storage and return capture offsets from traversal.

# Recommended Follow-up Work

Immediate high-value fixes:

1. Fix literal/query shadowing of parameter siblings.
2. Keep singleton required-query generated endpoints on unconstrained routes.
3. Finish the request lookup cleanup with heterogeneous `string_view` lookup.
4. Add tests for query-value serialization edge cases.
5. Add compile coverage for `noexcept` handlers and exact-value determinism.

Deeper refactors to evaluate:

1. direct compiled-route registration path
2. group-level generated dispatchers
3. indexed/query-mask leaf resolution
4. arena-backed trie storage

Profiling or measurement to run next:

1. microbenchmark `registry::find` for literal-only, parameterized, and same-path query-overloaded routes at 1/4/8/16 variants
2. allocation profiling of `registry::find` and generated request parsing
3. startup-time measurement of generated route registration before and after removing string round-tripping
4. compile-time and object-size comparison of generated models before and after builder/accessor emission changes

Tests or instrumentation to add later:

1. regression test for literal-leaf query failure falling through to a parameter sibling
2. regression test proving singleton required-query endpoints return `400`
3. compile tests for `noexcept` handlers
4. compile/runtime tests for exact-value query route determinism
5. tests covering query names/values containing `+`, `%`, and `&`

# Open Questions / Uncertainties

1. The singleton required-query `404` regression is a high-confidence control-flow inference, but I did not run a live non-skipped integration repro in this environment.
2. The compile-time exact-value ambiguity issue is proven at the type-system level but still needs explicit repository tests.
3. The serialization issue for special query characters is strongly suggested by the code path, but it was not exercised end-to-end here.
4. I did not benchmark the actual crossover point where leaf-local linear scans become a measurable bottleneck; that should be measured before selecting between a simple sort order and a mask/table redesign.
5. Several socket-based integration tests were skipped in this environment, so transport-level behavior was audited primarily by code inspection rather than live execution.
