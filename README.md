## Warp HTTP Framework

Warp is a WIP lib that uses Boost's Beast, Asio, and JSON to provide a high throughput, low footprint HTTP web server. Lots of work to be done.... 
- Boost Beast and Asio are completely abstracted away. Only Boost JSON is part of the public interface. 

### Build
```bash
cmake -S . -B build
cmake --build build --config Release
```

### Testing
Configure with tests enabled:
```bash
cmake -S . -B build -Dwarp_BUILD_TESTS=ON
```

Build the test targets:
```bash
cmake --build build --target warp_http_unit_tests warp_http_smoke_tests warp_http_integration_tests -j4
```

Run the unit tests:
```bash
./build/tests/warp_http_unit_tests
```

Run the smoke tests:
```bash
./build/tests/warp_http_smoke_tests
```

Run the integration tests:
```bash
./build/tests/warp_http_integration_tests
```

Notes:
- All unit tests use Google Test.
- Smoke tests also use Google Test, but avoid binding sockets.
- Integration tests use Google Test and start a real Warp server on `127.0.0.1`, so they need an environment where localhost binds are allowed.
- The DB integration tests use `GTEST_SKIP()` when `WARP_DB_USER`, `WARP_DB_PASSWORD`, and `WARP_DB_NAME` are not set.

### Run example server:
```bash
./build/examples/warp_example_server
Warp example server running on http://127.0.0.1:8080
Received a hello world request with query parameter name with value: Bob
```
### Run example request:
```bash
./build/examples/warp_example_request Bob | jq
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
- Construct the pool with an executor, then `co_await pool.async_query(...)` inside a route handler to fetch data without blocking the HTTP worker.
- You do not need to call `boost::asio::co_spawn` yourself. Warp spawns coroutine route handlers internally.
- Synchronous `query` is also available; it posts the work to the same database thread pool and blocks the caller until completion.
- Example:
  ```cpp
  .get("/db/{id}",
       [db_pool](warp::request req) -> warp::awaitable<warp::response> {
           auto id = req.path_param("id").value_or("");
           if (!is_integer(id)) {
               co_return warp::response::bad_request("id must be an integer");
           }

           try {
               auto result = co_await db_pool->async_query(
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
- robust Router
- metrics 
- (m)TLS support
- Request/response interceptors
- Return output as JSON
- Tag with request-id in header
- Code generation from YAML (using Jinja?) and allow registration of classes
- Add benchmark suite (Google Benchmark) and fuzz targets for parser hardening.
