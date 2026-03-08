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
- Construct the pool with an executor from your `io_context`, then `co_await pool.async_query(...)` inside a coroutine to fetch data without blocking the HTTP worker.
- Synchronous `query` is also available; it posts the work to the same database thread pool and blocks the caller until completion.
- Example:
  ```cpp
  boost::asio::io_context ctx;
  warp::db::postgres::connection_config cfg;
  cfg.database = "warp";
  warp::db::postgres::connection_pool pool(ctx.get_executor(), cfg, 8, 4);

  boost::asio::co_spawn(
      ctx,
      [&pool]() -> boost::asio::awaitable<void> {
          auto rows = co_await pool.async_query("select 1");
          if (rows.rows() > 0) {
              auto value = rows.value(0, 0);
              // use value...
          }
          co_return;
      },
      boost::asio::detached);

  ctx.run();
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
