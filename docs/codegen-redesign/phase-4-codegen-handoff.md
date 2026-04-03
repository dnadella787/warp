# Phase 4 Handoff

- files changed: `include/warp/codegen/data_class_emitter.hpp`, `include/warp/codegen/resource_stub_emitter.hpp`, `include/warp/codegen/resource_emitter.hpp`, `include/warp/codegen/stub_generator.hpp`, `include/warp/codegen/generator.hpp`, `src/codegen/data_class_emitter.cpp`, `src/codegen/resource_stub_emitter.cpp`, `src/codegen/resource_emitter.cpp`, `src/codegen/stub_generator.cpp`, `src/codegen/generator.cpp`, `src/codegen/codegen_cli.cpp`, `examples/codegen/generate_users_headers.cpp`
- decisions made: both emitters now take `api_model`; codegen no longer rebuilds validation in each emitter
- decisions made: generated resource adapters now use `users_api_routes<Service>` with captured `std::shared_ptr<Service>`
- decisions made: CLI and example header generator now write outputs atomically
- invariants: generated route callbacks never capture raw `this`
- invariants: generation path is `SpecAst -> ApiModel -> emitters`
- open issues: runtime adapter still needed structured media-type handling and router/request consistency work
- next phase input: align request parsing, router matching, and HTTP error mapping with the canonical route/runtime contract
