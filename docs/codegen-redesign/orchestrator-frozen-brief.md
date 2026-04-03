# Orchestrator Frozen Brief

Status: frozen source of truth for this orchestration pass. Update intent only by adding new handoffs, not by mutating this brief.

## Project Goal

Make the Warp HTTP codegen and runtime path production-grade by finishing and validating the redesign already implemented in this worktree. The resulting system must be lifetime-safe, deterministic, schema-safe, HTTP-correct, build-clean, and maintainable.

## Target Architecture

1. Parse YAML into a strict `SpecAst`/`api_spec`.
   - preserve source spans
   - reject duplicate keys
   - reject unknown keys
   - use shared route parsing rules
2. Canonicalize into one validated `api_model`.
   - own canonical routes, schemas, and symbol validation
   - reject duplicate normalized `(method, path-shape)` routes
   - validate HTTP body legality centrally
3. Emit only from `api_model`.
   - contract/data emitters are transport-neutral
   - Warp adapter emission handles request binding and route registration
4. Runtime uses the same route grammar and explicit ownership.
   - generated route binders capture `std::shared_ptr<Service>`
   - router registration and matching share canonical route parsing
   - request binding maps malformed input to `400` and media-type mismatches to `415`
5. Build graph is split cleanly.
   - `warp_codegen_core`
   - `warp_codegen_runtime`
   - `warp_codegen_cli`
   - `warp_http`
   - optional `warp_db_postgres`

## Invariants

- `api_model` is the single validated IR after parse.
- Emitters must not recompute validation rules independently.
- Validation and runtime registration must share route parsing and normalization.
- No silent route collisions or symbol collisions.
- Generated route callbacks must never capture raw `this`.
- Invalid specs must fail with structured diagnostics and source locations.
- Identical specs must generate byte-stable outputs.
- Request target refresh must clear and rebuild all derived request state atomically.
- Codegen-only consumers must not require PostgreSQL linkage.

## API Shape

- Parse/load entry points:
  - `parse_spec_ast(std::string_view)`
  - `load_spec_ast(const std::filesystem::path&)`
  - compatibility aliases `parse_api_spec(...)` and `load_api_spec(...)`
- Canonical model entry point:
  - `build_api_model(const spec_ast&, std::string_view namespace_override = {})`
- Generation entry points:
  - `stub_generator::{generate, generate_from_yaml, generate_from_file}`
  - `api_stub_generator::{generate, generate_from_yaml, generate_from_file}`
- Generated consumer shape:
  - `users_api_routes<Service>(std::shared_ptr<Service>)`
  - `void register_routes(warp::http::server_builder&) const`
- Runtime/request shape:
  - `warp::http::request::refresh_target_metadata()`
  - `set_path_params(...)`, `path_param(...)`, `query_param(...)`, `try_json_body()`
- Runtime/router shape:
  - `registry::add(method, std::string, handler|async_handler)`
  - `registry::find(request&)`
- Build integration shape:
  - `warp_generate_stubs(TARGET ... SPEC ... OUTPUT_DIR ... [NAMESPACE ...])`

## File Ownership Boundaries

- Parser/spec/diagnostics:
  - `include/warp/codegen/diagnostics.hpp`
  - `include/warp/codegen/spec_model.hpp`
  - `include/warp/codegen/spec_parser.hpp`
  - `src/codegen/spec_model.cpp`
  - `src/codegen/spec_parser.cpp`
  - `include/warp/common/route_pattern.hpp`
- Canonical model:
  - `include/warp/codegen/model.hpp`
  - `src/codegen/model.cpp`
- Emitters and codegen CLI:
  - `include/warp/codegen/data_class_emitter.hpp`
  - `include/warp/codegen/resource_emitter.hpp`
  - `include/warp/codegen/resource_stub_emitter.hpp`
  - `include/warp/codegen/stub_generator.hpp`
  - `include/warp/codegen/generator.hpp`
  - `src/codegen/data_class_emitter.cpp`
  - `src/codegen/resource_emitter.cpp`
  - `src/codegen/resource_stub_emitter.cpp`
  - `src/codegen/stub_generator.cpp`
  - `src/codegen/generator.cpp`
  - `src/codegen/codegen_cli.cpp`
- Runtime adapter/request/router:
  - `include/warp/codegen/http_adapter.hpp`
  - `include/warp/http/request.hpp`
  - `src/http/router/registry.hpp`
  - `src/http/router/registry.cpp`
- Build/tooling/examples:
  - `CMakeLists.txt`
  - `cmake/WarpCodegen.cmake`
  - `examples/CMakeLists.txt`
  - `examples/codegen/generate_users_headers.cpp`
  - `examples/codegen/users_resource_example.cpp`
- Tests:
  - `tests/unit/codegen/*`
  - `tests/unit/request_test.cpp`
  - `tests/unit/registry_test.cpp`
  - `tests/integration/generated_api_integration_test.cpp`
  - `tests/support/integration/http_integration_harness.*`
  - `tests/support/integration/http_db_test_support.*`
  - `tests/integration/http_db_integration_test.cpp`
  - `tests/CMakeLists.txt`

## Acceptance Criteria

- No critical or high-severity correctness, ownership, or API-misuse issues remain in the codegen/runtime path.
- Fresh no-DB configure/build/test works without requiring PostgreSQL or DB-only helpers.
- Parser rejects malformed specs with deterministic structured diagnostics and source spans.
- `build_api_model` enforces canonical route identity, symbol safety, and HTTP body legality.
- Generated route adapters use explicit shared ownership and keep behavior correct after service lifetime changes.
- Runtime binding and routing preserve the shared validation contract, including `400` vs `415`.
- Generated outputs are deterministic for identical specs.
- Risky paths have regression coverage, including negative cases.
- Remaining issues, if any, are explicitly low priority and non-blocking.

## Non-Goals

- Redesigning the overall HTTP server/session stack beyond codegen/runtime contract seams.
- Expanding generated error payloads beyond the current minimal `{error, code}` shape unless required to fix a blocker.
- Large API renames or aesthetic refactors that do not improve safety, determinism, or maintainability.
- Solving external PostgreSQL availability beyond making DB-specific tests skip or isolate cleanly.
- Spec feature expansion beyond the currently supported YAML subset.
