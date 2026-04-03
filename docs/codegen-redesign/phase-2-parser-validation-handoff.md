# Phase 2 Handoff

- files changed: `include/warp/codegen/diagnostics.hpp`, `include/warp/common/route_pattern.hpp`, `include/warp/codegen/spec_model.hpp`, `include/warp/codegen/spec_parser.hpp`, `src/codegen/spec_model.cpp`, `src/codegen/spec_parser.cpp`
- decisions made: kept `api_spec` as the strict AST carrier and added `spec_ast` alias for continuity
- decisions made: parser now rejects duplicate keys, unknown keys, ambiguous root forms, and invalid status strings
- decisions made: AST nodes now preserve source spans needed by later diagnostics
- decisions made: route parsing moved to shared `warp::common::parse_route_pattern`
- invariants: parser owns structural validation; later phases must not silently accept unknown or duplicate keys
- invariants: route shape parsing is shared and must remain canonical
- open issues: canonical symbol ownership and HTTP body legality still live in later phases
- next phase input: build `ApiModel` from the strict AST, enforce symbol collisions, canonical route identity, and status/body legality
