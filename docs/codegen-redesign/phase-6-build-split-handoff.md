# Phase 6 Handoff

- files changed: `CMakeLists.txt`, `cmake/WarpCodegen.cmake`, `tests/CMakeLists.txt`, `examples/CMakeLists.txt`
- decisions made: split targets into `warp_codegen_core`, `warp_codegen_runtime`, `warp_codegen_cli`, `warp_http`, and optional `warp_db_postgres`
- decisions made: `warp_http` no longer drags codegen or DB sources; codegen-only example links `warp_codegen_core`
- decisions made: `warp_generate_stubs(...)` now depends on the CLI target so generator rebuilds retrigger regeneration
- invariants: codegen-only consumers must not require PostgreSQL linkage
- invariants: generated header regeneration must follow CLI changes as well as spec changes
- open issues: DB integration remains environment-sensitive when local Postgres is unreachable
- next phase input: harden tests around parser/model/runtime/build behavior and complete the final verification pass
