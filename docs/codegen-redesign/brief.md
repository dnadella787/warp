# C++ HTTP Codegen Redesign Brief

Status: frozen source of truth for the redesign. Update intent only by adding phase handoffs, not by mutating this brief.

## Mission

Redesign and harden the Warp HTTP code generator and runtime so it is:

- lifetime-safe
- deterministic
- schema-safe
- HTTP-correct
- build-system clean
- testable
- maintainable

## Known Failures To Eliminate

- generated route lambdas capture raw `this`
- route identity is not canonicalized
- schema names can collide with generated symbols
- validation is too permissive
- HTTP semantics are incomplete
- there is no single validated IR
- build and package boundaries are wrong
- request state has split-state bugs
- generated output is too narrow and too heavy
- tests miss dangerous failure cases

## Required Target Architecture

1. Parse into a strict `SpecAst`.
   - preserve source spans
   - reject duplicate keys
   - reject unknown keys

2. Canonicalize into one validated `ApiModel`.
   - owns the symbol table
   - owns canonical route patterns
   - owns schema registry
   - validates HTTP semantics

3. Emit only from `ApiModel`.
   - one transport-neutral contract emitter
   - one Warp adapter emitter

4. Replace unsafe resource binding.
   - no raw `this` capture
   - explicit ownership
   - prefer `std::shared_ptr<Service>` binder or adapter shapes

5. Canonical route identity.
   - one parser
   - normalized route shapes
   - reject duplicate normalized `(method, path-shape)` entries

6. Symbol safety.
   - global symbol table
   - fail fast on collisions

7. HTTP correctness.
   - validate body and status legality
   - distinguish `400 Bad Request` from `415 Unsupported Media Type`
   - structured diagnostics

8. Build split.
   - `warp_codegen_core`
   - `warp_codegen_runtime`
   - `warp_codegen_cli`
   - `warp_http`
   - optional DB modules

## Mandatory Invariants

- one canonical representation after parse: `ApiModel`
- emitters never recompute validation rules independently
- route pattern parsing is shared between validation and runtime registration
- no silent route collisions
- no silent symbol collisions
- no dangling route callbacks after service destruction
- invalid specs fail with structured diagnostics and source locations
- generated outputs are deterministic for identical specs

## Repository Anchors

- parser/spec: `include/warp/codegen/spec_parser.hpp`, `src/codegen/spec_parser.cpp`, `include/warp/codegen/spec_model.hpp`
- current model normalization: `include/warp/codegen/model.hpp`, `src/codegen/model.cpp`
- emitters: `src/codegen/data_class_emitter.cpp`, `src/codegen/resource_stub_emitter.cpp`, `include/warp/codegen/http_adapter.hpp`
- runtime: `include/warp/http/server.hpp`, `src/http/server.cpp`, `include/warp/http/request.hpp`, `src/http/router/registry.cpp`
- build: `CMakeLists.txt`, `cmake/WarpCodegen.cmake`, `tests/CMakeLists.txt`

## Phase Workflow

1. Architecture and IR
2. Parser and validation
3. Canonical model
4. Code generation
5. Runtime ownership and routing
6. Build and CMake split
7. Tests and regression hardening
