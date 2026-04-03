# Phase 5 Handoff

- files changed: `include/warp/codegen/http_adapter.hpp`, `include/warp/http/request.hpp`, `src/http/router/registry.hpp`, `src/http/router/registry.cpp`
- decisions made: request metadata refresh now clears stale path params
- decisions made: router registration and matching now use the shared canonical route parser
- decisions made: runtime rejects duplicate normalized route shapes during registration
- decisions made: path params are percent-decoded before binding
- decisions made: binding errors now carry HTTP status; unsupported media type maps to `415`, malformed input stays `400`
- invariants: request path/query derived state is refreshed atomically from the target
- invariants: route matching and model validation share one route grammar
- open issues: response bodies are model-validated and runtime-guarded, but generated error payloads still use a minimal `{error, code}` shape
- next phase input: split build targets cleanly and wire regeneration dependencies through the new CLI/core boundaries
