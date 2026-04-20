## Warp HTTP Framework

Warp is a C++20 HTTP framework built on Boost.Beast, Asio, and Boost.JSON.

The current tree provides:

- an HTTP server builder with sync and coroutine route handlers
- request parsing helpers for path, query, and JSON body access
- an optional PostgreSQL integration layer
- a YAML-to-C++ code generator for typed request/response and resource stubs
- unit, smoke, integration, and benchmark targets

### Build
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dwarp_BUILD_TESTS=ON \
  -Dwarp_BUILD_EXAMPLES=ON
cmake --build build -j4
```

Key CMake options:

- `warp_BUILD_TESTS=ON|OFF` builds the GoogleTest targets
- `warp_BUILD_EXAMPLES=ON|OFF` builds the example programs
- `warp_BUILD_BENCHMARKS=ON|OFF` builds the Google Benchmark target
- `warp_BUILD_DB=ON|OFF` enables the PostgreSQL module and DB-backed examples/tests

On Apple Silicon, prefer adding `-DCMAKE_OSX_ARCHITECTURES=arm64` when configuring a fresh tree.

### Testing
Configure with tests enabled:
```bash
cmake -S . -B build-test \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dwarp_BUILD_DB=ON \
  -Dwarp_BUILD_TESTS=ON \
  -Dwarp_BUILD_EXAMPLES=OFF \
  -Dwarp_BUILD_BENCHMARKS=OFF
```

Build and run the full suite:
```bash
cmake --build build-test -j4
ctest --test-dir build-test --output-on-failure
```

The main test executables are:

```bash
./build-test/tests/warp_http_unit_tests
./build-test/tests/warp_http_smoke_tests
./build-test/tests/warp_http_integration_tests
```

Notes:
- All unit tests use Google Test.
- Smoke tests also use Google Test, but avoid binding sockets.
- Integration tests use Google Test and start a real Warp server on `127.0.0.1`, so they need an environment where localhost binds are allowed.
- DB integration tests are only built when `warp_BUILD_DB=ON`.
- The DB integration tests use `GTEST_SKIP()` when `WARP_DB_USER`, `WARP_DB_PASSWORD`, and `WARP_DB_NAME` are not set.

### Run example server:
```bash
# Optional: only needed if you want the /db/{id} route to succeed.
export WARP_DB_HOST=127.0.0.1
export WARP_DB_PORT=5432
export WARP_DB_USER=...
export WARP_DB_PASSWORD=...
export WARP_DB_NAME=...

./build/examples/warp_example_server
```

`warp_example_server` is only built when `warp_BUILD_DB=ON`, because it includes the `/db/{id}` route. The `/hello` and `/ping` routes still work without DB env vars.

### Run example request:
```bash
./build/examples/warp_example_request /hello/Bob
./build/examples/warp_example_request '/hello?name=Bob' | jq
./build/examples/warp_example_request /ping | jq
./build/examples/warp_example_request /db/NYSE | jq
```

The request example takes a raw HTTP request target as `argv[1]`, not just a bare name.

Expected JSON for the query-string example:

```json
{
  "name": "Bob"
}
```
### Run clang-format
```bash
clang-format -i $(git ls-files '*.cpp' '*.hpp')
```

### PostgreSQL Client
- `warp::db::postgres::connection_pool` wraps libpqxx and runs every query on a dedicated background thread pool so request handlers stay non-blocking.
- Construct the pool with an executor, then `co_await pool.query(...)` inside a route handler to fetch data without blocking the HTTP worker.
- You do not need to call `boost::asio::co_spawn` yourself. Warp spawns coroutine route handlers internally.
- `query(...)` returns `boost::asio::awaitable<result>` and posts the SQL work onto the pool's worker threads.
- Example:
  ```cpp
  .get("/db/{id}",
       [db_pool](warp::request req) -> warp::awaitable<warp::response> {
           auto id = req.path_param("id").value_or("");
           if (!is_integer(id)) {
               co_return warp::response::bad_request("id must be an integer");
           }

           try {
               auto result = co_await db_pool->query(
                   std::string("select ") + std::string(id) +
                   "::int as requested_id, current_database() as database_name");
               co_return warp::response::ok(
                   warp::body_builder()
                       .set("requested_id",
                            result.rows() > 0 ? std::string(result.value(0, 0)) : std::string(id))
                       .set("database_name",
                            result.rows() > 0 ? std::string(result.value(0, 1)) : std::string {})
                       .build());
           } catch (const std::exception &ex) {
               co_return warp::response::server_error(ex.what());
           }
       })
  ```

### TODO 
- AuthN/Z 
- throttling based on available socket FDs on system
- metrics 
- (m)TLS support
- client lib support
- Request/response interceptors
- Tag with request-id in header
- Fuzz targets for parser hardening.

### Code generation

Warp can generate typed request/response models and route-registration adapters from a YAML API description. The generator surface lives under `warp::codegen`.

Two supported workflows:

- ahead of time with the `warp_codegen` CLI
- during the build with the `warp_generate_stubs(...)` CMake function

- Docs: [docs/codegen.md](/Users/dnadella/Projects/warp/docs/codegen.md)
- Example YAML: [examples/codegen/users_api.yaml](/Users/dnadella/Projects/warp/examples/codegen/users_api.yaml)
- Example usage: [examples/codegen/users_resource_example.cpp](/Users/dnadella/Projects/warp/examples/codegen/users_resource_example.cpp)

### Benchmarking

Warp includes a Google Benchmark target for event-loop round-trip measurements under sustained concurrent load.

```bash
cmake -S . -B build-bench \
  -DCMAKE_BUILD_TYPE=Release \
  -Dwarp_BUILD_DB=ON \
  -Dwarp_BUILD_BENCHMARKS=ON \
  -Dwarp_BUILD_TESTS=OFF \
  -Dwarp_BUILD_EXAMPLES=OFF
cmake --build build-bench --target warp_http_event_loop_benchmark -j4
./build-bench/benchmarks/warp_http_event_loop_benchmark \
  --warp-benchmark-concurrency=1k,5k,10k \
  --warp-benchmark-client-threads=8 \
  --warp-benchmark-warmup=5s \
  --warp-benchmark-duration=60s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true \
  --benchmark_counters_tabular=true
```
You can optionally enable the sync db query as well (it may error out, read the benchmarking doc for more info) using ``
If your shell is running under Rosetta on Apple Silicon, prefix configure, build, and run with `arch -arm64`.

See [docs/benchmarking.md](/Users/dnadella/Projects/warp/docs/benchmarking.md) for DB setup and interpretation notes.
