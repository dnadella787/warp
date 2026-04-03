# Discover Handoff

- files changed: added redesign documentation only
- decisions made: current pipeline is `parse -> api_spec -> emitter-local build_api_model -> string emitters -> runtime`
- decisions made: there is no single retained validated IR
- decisions made: runtime route parsing and model route parsing are separate implementations
- decisions made: generated resource bases capture raw `this` in route lambdas
- decisions made: `warp_http` currently mixes codegen, runtime, and DB code
- invariants: future phases must use `brief.md` as the frozen source of truth
- invariants: shared interface changes must be serialized before dependent work
- open issues: duplicate keys are not rejected
- open issues: unknown keys are not rejected
- open issues: route identity is not canonicalized across model and runtime
- open issues: request target refresh does not clear all derived state
- open issues: HTTP semantics and content-type handling are incomplete
- next phase input: define the canonical architecture, core IR types, diagnostics model, ownership model, and implementation order
