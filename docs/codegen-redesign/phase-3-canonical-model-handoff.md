# Phase 3 Handoff

- files changed: `include/warp/codegen/model.hpp`, `src/codegen/model.cpp`
- decisions made: `api_model` is now the single validated IR and stores namespace, canonical routes, and response body mode
- decisions made: removed public-name auto-suffixing; collisions now fail through one global symbol table
- decisions made: duplicate normalized `(method, path-shape)` routes are hard errors
- decisions made: `204`, `205`, `304`, and `1xx` responses with bodies are rejected in model construction
- invariants: emitters must consume only `api_model`
- invariants: no silent symbol drift from unrelated schema or endpoint additions
- open issues: emitters still needed to be switched off direct spec consumption
- next phase input: emit contracts and Warp adapters from `ApiModel` only and replace raw-`this` resource registration
