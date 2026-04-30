## Warp HTTP Framework

Warp is a C++20 HTTP framework built on Boost.Beast, Asio, and Boost.JSON.

Warp is licensed under the [MIT License](LICENSE).

The current tree includes:

- an HTTP server builder with sync and coroutine route handlers
- callback and coroutine internal event-loop implementations
- request parsing helpers for path, query, header, and JSON body access
- query-aware routing shared across runtime, typed, and generated endpoints
- an optional PostgreSQL integration layer
- a YAML-to-C++ code generator for typed request/response models and resource stubs
- unit, smoke, integration, and benchmark targets

### Recent Updates

Highlights from the latest changes in this checkout:

- `2026-04-21`: query-route semantics were unified across runtime routing, typed routes, and generated resources.
- `2026-04-21`: singleton generated endpoints with required query parameters now preserve binder-driven `400` responses instead of failing routing early.
- `2026-04-21`: route registration now uses compiled route metadata directly, and request parameter lookup supports heterogeneous `std::string_view` keys.
- `2026-04-20`: generated models now expose builders plus setter/getter accessors, and generated handlers may return either typed responses or raw `warp::response` values.
- `2026-04-20`: `server_impl` shutdown handling picked up fixes for run/stop races and self-join issues.
- `2026-04-18`: coroutine session shutdown was tightened to handle server-side close scenarios more reliably.

### Build

Requirements:

- CMake `3.20+`
- a C++20 compiler
- Boost `1.90+` with `Boost::json`
- PostgreSQL client libraries only when `warp_BUILD_DB=ON`

Configure and build:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dwarp_BUILD_TESTS=ON \
  -Dwarp_BUILD_EXAMPLES=ON \
  -Dwarp_BUILD_DB=ON
cmake --build build -j4
```

Useful CMake options:

- `warp_BUILD_TESTS=ON|OFF` builds the GoogleTest targets
- `warp_BUILD_EXAMPLES=ON|OFF` builds the example programs
- `warp_BUILD_BENCHMARKS=ON|OFF` builds the Google Benchmark target
- `warp_BUILD_DB=ON|OFF` enables the PostgreSQL module and DB-backed examples/tests
- `warp_FETCH_BOOST=ON|OFF` fetches Boost with `FetchContent` when a suitable local install is unavailable
- `WARP_BENCHMARK_SYNC_DB=ON|OFF` enables the synchronous DB benchmark variant

### Logging

Warp exposes a small `warp::log` wrapper over `spdlog`. In normal use you configure sinks through `warp::log::sink`, create a `warp::log::logger`, and optionally install it in two places:

- `logger.set_as_default()` for application-level `warp::log::info(...)` style logging
- `warp::server::server_builder::logger(...)` for Warp server internals such as listener/session/server error paths

```cpp
#include <warp/warp.hpp>
#include <warp/server/server_builder.hpp>

int main() {
    auto app_logger = warp::log::logger::stderr_color("my-app");
    app_logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    app_logger.set_level(warp::log::level::info);
    app_logger.set_as_default();

    auto server = warp::server::server_builder()
        .logger(app_logger)
        .get<"/health">([](const warp::request &) -> warp::response {
            return warp::response::ok("ok");
        })
        .build();

    warp::log::info("application startup");
}
```

`warp::server::server_builder::logger(...)` captures the logger object that server internals will use when `build()` runs. If you omit `.logger(...)`, `build()` captures the then-current `warp::log::default_logger()` instead. Later `warp::log::set_default_logger(...)` or `logger.set_as_default()` calls affect future default-log lookups and future server builds, but they do not retarget servers that were already built.

For a multi-sink logger that writes to both stdout and a file, see `examples/custom_logger_server.cpp` and run:

```bash
./build/examples/warp_custom_logger_server
```

That example uses:

```cpp
auto combined_logger = warp::log::logger("warp.custom_logger_server",
                                         {warp::log::sink::stdout_color(),
                                          warp::log::sink::basic_file("warp-custom-server.log", true)});
combined_logger.flush_on(warp::log::level::info);
combined_logger.set_as_default();

auto server = warp::server::server_builder()
    .logger(combined_logger)
    // routes...
    .build();
```

`warp::log::sink::basic_file("warp-custom-server.log", true)` truncates the file on startup so each run starts from a clean log. The file sink is otherwise buffered; the example enables `flush_on(info)` so `tail -f warp-custom-server.log` shows new requests before shutdown instead of only after buffered output is flushed later.

On Apple Silicon, prefer adding `-DCMAKE_OSX_ARCHITECTURES=arm64` when configuring a fresh tree. If your shell is running under Rosetta, prefix configure, build, and run commands with `arch -arm64`.

### Testing

Configure a test build:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dwarp_BUILD_DB=ON \
  -Dwarp_BUILD_TESTS=ON \
  -Dwarp_BUILD_EXAMPLES=OFF \
  -Dwarp_BUILD_BENCHMARKS=OFF
```

Build and run the full suite:

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Main test executables:

```bash
./build/tests/warp_http_unit_tests
./build/tests/warp_http_smoke_tests
./build/tests/warp_http_integration_tests
```

Notes:

- unit and smoke tests use GoogleTest
- smoke tests avoid binding sockets
- integration tests start a real Warp server on `127.0.0.1`
- DB integration tests are only built when `warp_BUILD_DB=ON`
- DB integration tests skip automatically when `WARP_DB_USER`, `WARP_DB_PASSWORD`, or `WARP_DB_NAME` are unset

### Run The Example Server

`warp_example_server` is only built when `warp_BUILD_DB=ON`, because it includes the `/db/{id}` route. The `/hello` and `/ping` routes still work even if DB environment variables are not set.

For a local Dockerized PostgreSQL instance that also seeds the `exchanges` table used by the example route and DB benchmarks, see [docs/postgres-local.md](docs/postgres-local.md).

```bash
export WARP_DB_HOST=127.0.0.1
export WARP_DB_PORT=5432
export WARP_DB_USER=...
export WARP_DB_PASSWORD=...
export WARP_DB_NAME=...

./build/examples/warp_example_server
```

### Run The Custom Logger Example

`warp_custom_logger_server` is a dedicated logging example. It configures a logger with both stdout and file sinks, truncates `warp-custom-server.log` on startup, enables `flush_on(info)` so the file updates while the server is running, installs that logger as the default app logger, and passes it into `server_builder::logger(...)` so the server internals capture the same logger at build time.

```bash
./build/examples/warp_custom_logger_server
cat warp-custom-server.log
```

### Run Example Requests

```bash
./build/examples/warp_example_request /hello/Bob
./build/examples/warp_example_request '/hello?name=Bob' | jq
./build/examples/warp_example_request /ping | jq
./build/examples/warp_example_request /db/NYSE | jq
```

`warp_example_request` sends a raw HTTP target as `argv[1]`, not just a bare name.

Expected JSON for the query-string example:

```json
{
  "name": "Bob"
}
```

### PostgreSQL Client

- `warp::db::postgres::connection_pool` wraps libpqxx and runs SQL work on a dedicated background thread pool
- construct the pool with an executor, then `co_await pool.query(...)` inside a route handler
- Warp spawns coroutine route handlers internally, so you do not need to call `boost::asio::co_spawn` yourself
- `query(...)` returns `boost::asio::awaitable<result>` and resumes on the configured completion executor

Example:

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

### Code Generation

Warp can generate typed request/response models and route-registration adapters from a YAML API description. The generator surface lives under `warp::codegen`.

Two supported workflows:

- ahead of time with the `warp_codegen` CLI
- during the build with the `warp_generate_stubs(...)` CMake function

Related docs and examples:

- [Code generation guide](docs/codegen.md)
- [Example YAML spec](examples/codegen/users_api.yaml)
- [Generated-resource example](examples/codegen/users_resource_example.cpp)

Build the CLI:

```bash
cmake --build build --target warp_codegen_cli -j4
```

Generate headers:

```bash
./build/warp_codegen \
  --spec examples/codegen/users_api.yaml \
  --output-dir generated
```

### Benchmarking

Warp includes a Google Benchmark target for comparing callback and coroutine event-loop overhead under sustained concurrent load.

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -Dwarp_BUILD_DB=ON \
  -Dwarp_BUILD_BENCHMARKS=ON \
  -Dwarp_BUILD_TESTS=OFF \
  -Dwarp_BUILD_EXAMPLES=OFF
cmake --build build --target warp_http_event_loop_benchmark -j4
./build/benchmarks/warp_http_event_loop_benchmark \
  --warp-benchmark-concurrency=1k,5k,10k \
  --warp-benchmark-client-threads=8 \
  --warp-benchmark-warmup=5s \
  --warp-benchmark-duration=60s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true \
  --benchmark_counters_tabular=true
```

If you want the synchronous DB benchmark variant as well, configure with `-DWARP_BENCHMARK_SYNC_DB=ON`.

See [docs/benchmarking.md](docs/benchmarking.md) for benchmark setup, DB prerequisites, and interpretation notes.

### Additional Documentation

- [Threading model](docs/threading-model.md)
- [Registry and query routing](docs/registry.md)
- [Coroutines](docs/coroutines.md)
- [Benchmarking](docs/benchmarking.md)

### Roadmap

- AuthN/Z
- throttling based on available socket FDs on system
- metrics
- (m)TLS support
- client library support
- request/response interceptors
- tag requests with a request ID header
- fuzz targets for parser hardening
