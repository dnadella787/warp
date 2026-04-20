# YAML Stub Generation

Warp includes a code generation pipeline that turns a constrained YAML API description into:

- JSON request/response body structs
- typed endpoint request envelopes
- generated route-registration adapters grouped by resource
- request parsing and response serialization glue for `warp::request` / `warp::response`

## YAML format

Top-level forms:

- `resources:` for explicit resource grouping
- `name:` + `endpoints:` for a single default resource
- optional `cpp_namespace:` to control the generated C++ namespace
- optional `namespace:` as an alias for `cpp_namespace:`

Example:

```yaml
cpp_namespace: generated_api
resources:
  - name: users
    endpoints:
      - name: create_user
        method: POST
        path: /users/{user_id}
        request:
          parameters:
            - name: user_id
              in: path
              type: string
            - name: verbose
              in: query
              type: bool
              required: false
            - name: x-trace-id
              in: header
              type: string
          body:
            type: object
            fields:
              - name: name
                type: string
              - name: nickname
                type: string
                required: false
        response:
          status: 201
          body:
            type: object
            fields:
              - name: id
                type: int64
              - name: active
                type: bool
      - name: health
        method: GET
        path: /health
        response:
          status: 204
```

Schema rules:

- Primitive types: `string`, `int64`, `double`, `bool`
- Object schemas use `type: object` with `fields:`
- Array schemas use `type: array` with `items:`
- Request parameters must be primitive
- Path parameters must be present in both the route path and the request parameter list
- Nullable schemas are reserved for future support and currently rejected during model normalization
- Generated type names and route-adapter aliases use `snake_case`
- Generated C++ member names are sanitized to valid identifiers while preserving the wire name for binding, for example `x-trace-id` becomes `x_trace_id`

## Ahead-of-time generation with the CLI

Build the CLI:

```bash
cmake -S . -B build
cmake --build build --target warp_codegen_cli -j4
```

After `cmake --install`, the binary is installed as `bin/warp_codegen`.

Generate headers into a source-controlled or shared output directory:

```bash
./build/warp_codegen \
  --spec examples/codegen/users_api.yaml \
  --output-dir generated
```

Optional CLI flags:

- `--namespace <cpp-namespace>` overrides the namespace from the YAML file
- `--model-header <filename>` changes the generated request/response header name
- `--resource-header <filename>` changes the generated resource-base header name

This is the preferred path when you want to generate code once, review it, and check the headers into your own repository.

## Compile-time generation with CMake

The package exposes a `warp_generate_stubs(...)` CMake function. It is available:

- automatically after `add_subdirectory(...)` on the warp source tree
- after `find_package(warp CONFIG REQUIRED)` from an installed package

Example:

```cmake
find_package(warp CONFIG REQUIRED)

warp_generate_stubs(
    TARGET users_api_codegen
    SPEC ${CMAKE_CURRENT_SOURCE_DIR}/users_api.yaml
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
    MODEL_HEADER users_api_types.hpp
    RESOURCE_HEADER users_api_resources.hpp
    OUT_MODEL_HEADER USERS_API_TYPES
    OUT_RESOURCE_HEADER USERS_API_RESOURCES
)

add_executable(users_server server.cpp)
add_dependencies(users_server users_api_codegen)
target_include_directories(users_server PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(users_server PRIVATE warp::warp_http)
```

`warp_generate_stubs(...)` supports:

- required: `TARGET`, `SPEC`, `OUTPUT_DIR`
- optional: `NAMESPACE`, `MODEL_HEADER`, `RESOURCE_HEADER`
- optional output variables: `OUT_MODEL_HEADER`, `OUT_RESOURCE_HEADER`
- optional extra dependencies: `DEPENDS`

Use this path when the generated headers should live in the build tree and stay in sync with the YAML spec automatically.

## Programmatic generation

The generator library API remains available when you want to embed generation into your own tools:

```cpp
#include "warp/codegen/generator.hpp"

#include <fstream>

int main() {
    auto generated = warp::codegen::api_stub_generator().generate_from_file("users_api.yaml");
    std::ofstream("generated_api_types.hpp") << generated.model_header;
    std::ofstream("generated_api_resources.hpp") << generated.resource_header;
}
```

## Generated artifacts

`warp::codegen::api_stub_generator` emits two headers:

- model header: JSON body structs, typed request envelopes, typed result structs
- resource header: request/response contract aliases and a generated route-registration alias per resource

The generated resource adapter shape is:

```cpp
template <typename Service>
using users_api_routes = warp::codegen::generated_resource<
    Service,
    users_create_user_request_endpoint<Service>,
    users_health_request_endpoint<Service>>;
```

The resource class implements handlers like:

```cpp
class users_resource {
public:
	explicit users_resource() = default;
	generated_api::users_create_user_response create_user(generated_api::users_create_user_request request);
} 
```

Handlers may return either the generated result type directly or `warp::awaitable<result_type>`. Route registration dispatches through `warp::codegen::invoke_user_handler`, so sync and coroutine handlers both work.

Register the class with the server builder like:
```cpp
int main() {
	auto service = std::make_shared<users_resource>();
	generated_api::users_api_routes routes(service);
	auto server = warp::http::server_builder().address("127.0.0.1").port(8080).register_resource(routes).build();
	server.run();
	return 0;
}
```

## Request parsing and response serialization

Each generated endpoint binds the incoming `warp::request` exactly once into a typed request envelope:

- path/query/header parameters are parsed with explicit type checks
- JSON bodies require `Content-Type: application/json` and are converted with `boost::json::value_to`
- invalid bindings become `400 Bad Request` responses with an error payload

Responses are serialized from the generated result type using `response_contract_traits`:

- endpoints with a body serialize JSON automatically
- endpoints without a body emit the configured HTTP status and an empty body

## Adding new endpoints or resources

1. Add a new resource or endpoint entry to the YAML file.
2. Regenerate headers with either `warp_codegen` or `warp_generate_stubs(...)`.
3. Implement the generated request/response handler signatures on your service class.
4. Register the derived resource with `server_builder`.

## Running tests

```bash
cmake -S . -B build-test -Dwarp_BUILD_EXAMPLES=OFF -Dwarp_BUILD_BENCHMARKS=OFF
cmake --build build-test --target warp_http_unit_tests warp_http_integration_tests -j4
./build-test/tests/warp_http_unit_tests --gtest_filter='ApiModelTest.*:SpecParserTest.*:DataClassEmitterTest.*:ApiStubGeneratorTest.*:ResourceEmitterTest.*'
./build-test/tests/warp_http_integration_tests --gtest_filter='GeneratedApiIntegrationTest.*'
```

## Example files

- example YAML: [examples/codegen/users_api.yaml](/Users/dnadella/Projects/warp/examples/codegen/users_api.yaml)
- CLI example target source: [codegen_cli.cpp](/Users/dnadella/Projects/warp/src/codegen/codegen_cli.cpp)
- CMake helper module: [WarpCodegen.cmake](/Users/dnadella/Projects/warp/cmake/WarpCodegen.cmake)
- header generation example: [examples/codegen/generate_users_headers.cpp](/Users/dnadella/Projects/warp/examples/codegen/generate_users_headers.cpp)
- illustrative consumer flow: [examples/codegen/users_resource_example.cpp](/Users/dnadella/Projects/warp/examples/codegen/users_resource_example.cpp)
- runtime integration test: [generated_api_integration_test.cpp](/Users/dnadella/Projects/warp/tests/integration/generated_api_integration_test.cpp)
