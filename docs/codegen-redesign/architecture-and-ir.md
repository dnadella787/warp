# Architecture And IR

## Scope

This phase defines the shared interfaces and invariants that all implementation phases must follow. It intentionally does not change production code.

## Module Layout

Introduce four primary layers:

1. `SpecAst`
   - strict parse tree for the supported YAML subset
   - preserves source spans for every user-authored node
   - validates structure, duplicate keys, and unknown keys

2. `ApiModel`
   - canonical validated API description
   - no parser artifacts, no duplicate identities, no unresolved names
   - sole input to emitters

3. emitters
   - contract emitter: transport-neutral request and response contracts
   - Warp adapter emitter: request binding, route registration, handler dispatch

4. runtime support
   - route pattern parser shared with model validation
   - structured request binding errors
   - ownership-safe service binding helpers

## New Core Types

```cpp
namespace warp::codegen {

struct source_span {
    std::size_t line = 0;
    std::size_t column = 0;
};

struct diagnostic {
    enum class severity { error };
    std::string code;
    std::string message;
    source_span span;
};

class diagnostic_sink {
public:
    void error(std::string code, source_span span, std::string message);
    [[nodiscard]] bool has_errors() const noexcept;
    [[nodiscard]] const std::vector<diagnostic> &items() const noexcept;
};

} // namespace warp::codegen
```

`SpecAst` owns spans and exact author intent. `ApiModel` owns canonical names, routes, schemas, and HTTP semantics.

## SpecAst Shape

`SpecAst` should model only supported syntax:

- `SpecAst`
- `ResourceAst`
- `EndpointAst`
- `RequestAst`
- `ResponseAst`
- `ParameterAst`
- `SchemaAst`
- `FieldAst`

Each node carries a `source_span`. Mapping nodes must be parsed against an allowed-key set. Duplicate keys are rejected during parse, not later normalization.

## ApiModel Shape

`ApiModel` should be the only validated IR consumed by emitters:

```cpp
namespace warp::codegen {

enum class http_body_mode { forbidden, optional, required };

struct route_segment {
    enum class kind { literal, parameter };
    kind kind = kind::literal;
    std::string text;
};

struct route_pattern {
    std::string original_path;
    std::vector<route_segment> segments;
    std::string shape_key;
};

struct symbol_id {
    std::string canonical;
    source_span declared_at;
};

struct schema_ref {
    std::string name;
};

struct SchemaModel;
struct EndpointModel;
struct ResourceModel;

struct ApiModel {
    std::string cpp_namespace;
    std::vector<SchemaModel> schemas;
    std::vector<ResourceModel> resources;
};

} // namespace warp::codegen
```

`ApiModel` owns:

- a global symbol table for emitted type and member names
- a canonical route registry keyed by `(method, shape_key)`
- a schema registry keyed by canonical schema name
- validated request and response semantics

## Canonical Route Identity

Use one route parser for both validation and runtime.

Normalization rules:

- path must start with `/`
- no empty segments except the root path
- each segment is either a literal or `{parameter}`
- literals keep exact wire spelling
- parameters normalize to a positional placeholder in the shape key
- `GET /users/{id}` and `GET /users/{name}` have the same shape key and must collide

Example:

- original path: `/users/{user_id}/posts/{post_id}`
- shape key: `/users/{}/posts/{}`

The route parser must also produce parameter order and names for request binding.

## Symbol Table Rules

Introduce one global symbol registry during `ApiModel` construction.

Tracked categories:

- schema type names
- request and response contract names
- resource adapter names
- handler method names within a resource
- generated member names within a type

Rules:

- user-specified names are canonicalized once
- collisions are hard errors
- suffixing is allowed only for internal synthetic helper names that are not part of the public contract
- public emitted type names never silently drift because of unrelated additions elsewhere

## HTTP Semantics

Validate in `ApiModel`, not in emitters:

- body legality for request methods and response status codes
- `204`, `205`, and `304` must not declare a body
- status strings and numeric status codes must normalize to one internal representation
- body bindings must distinguish malformed payloads from unsupported media type

Runtime diagnostics should classify at least:

- malformed path, query, or header input -> `400`
- malformed JSON -> `400`
- unsupported or missing media type when JSON is required -> `415`

## Ownership Model For Generated Resources

Replace CRTP bases that register raw `this` callbacks with explicit binders.

Preferred generated shape:

```cpp
template <typename Service>
class users_api_routes {
public:
    explicit users_api_routes(std::shared_ptr<Service> service);
    void register_routes(warp::http::server_builder &builder) const;
private:
    std::shared_ptr<Service> service_;
};
```

Properties:

- route callbacks capture `std::shared_ptr<Service>` by value
- callback lifetime is decoupled from stack lifetime of the service object
- user code becomes explicit: construct service, wrap it, register routes

CRTP can remain only as a thin compile-time contract helper if it does not own registration.

## Emission Split

Emit only from `ApiModel`.

Contract emitter responsibilities:

- schema structs
- request contracts
- response contracts
- JSON conversion helpers

Warp adapter emitter responsibilities:

- request binding traits or functions
- response serialization traits or functions
- route registration adapters using shared ownership

Emitters must never:

- parse paths
- validate schema legality
- invent names independently

## Build Boundaries

Target split:

- `warp_codegen_core`: parser, diagnostics, route parser, `SpecAst`, `ApiModel`, emitters
- `warp_codegen_runtime`: HTTP adapter helpers used by generated code
- `warp_codegen_cli`: CLI only, linked against `warp_codegen_core`
- `warp_http`: server, request, response, router
- optional DB targets independent from codegen-only flows

Generated consumers that only use codegen should not pull in PostgreSQL.

## Tradeoffs

- strict parse rejection is less forgiving, but it prevents ambiguous specs and emitter-specific behavior
- a retained `ApiModel` increases implementation surface, but removes duplicate logic and drift
- a shared route parser constrains flexibility, but guarantees validation and runtime matching agree
- explicit shared ownership is slightly heavier than raw capture, but it removes dangling callback hazards

## Serialization And Parallelism

Serialize these phases:

- Architecture and IR
- Parser and validation
- Canonical model
- Runtime ownership and routing

These can overlap later once interfaces stabilize:

- Build and CMake split with parts of code generation cleanup
- Test expansion against already-settled behavior

## Implementation Order

1. add diagnostics primitives and route pattern parser API
2. add strict `SpecAst` types and parser validation
3. add `ApiModel` builder with symbol table, schema registry, and route registry
4. switch emitters to consume only `ApiModel`
5. replace generated resource bases with shared-ownership route binders
6. update runtime request and routing state to use shared route canonicalization
7. split CMake targets and regenerate integration wiring
8. add negative tests, generated compile tests, and regeneration smoke tests

## Phase Exit Criteria

The next phase may start once the following are accepted:

- `SpecAst`, `ApiModel`, diagnostics, and route parser responsibilities are fixed
- raw `this` capture is officially removed from the target design
- emitters are defined as `ApiModel` consumers only
- route identity is defined by normalized shape, not original parameter spelling
