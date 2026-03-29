## Warp HTTP Framework

Warp is a WIP lib that uses Boost's Beast, Asio, and JSON to provide a high throughput, low footprint HTTP web server. Lots of work to be done.... 
- Boost Beast and Asio are completely abstracted away. Only Boost JSON is part of the public interface. 

### Build
```bash
cmake -S . -B build
cmake --build build --config Release
```
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
- Lots of unit tests (with GoogleTest)
- Code generation from YAML (using Jinja?) and allow registration of classes
- Add benchmark suite (Google Benchmark) and fuzz targets for parser hardening.
