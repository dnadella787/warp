## Warp HTTP Framework

Warp is a WIP lib that uses Boost's Beast, Asio, and JSON to provide a high throughput, low footprint HTTP web server. Lots of work to be done.... 
- Boost (beast, asio, json) types left internal 

### Build
```bash
cmake -S . -B build
cmake --build build --config Release
```
### Run example server:
```bash
./build/examples/warp_example_server
```
### Run example request:
```bash
./build/examples/warp_example_request Bob
```

### TODO 
- robust Router
- metrics 
- (m)TLS support
- Request/response interceptors
- Return output as JSON
- Tag with request-id in header
- Lots of unit tests (with GoogleTest)
- Revisit json_value
- Code generation from YAML (using Jinja?) and allow registration of classes
- Add benchmark suite (Google Benchmark) and fuzz targets for parser hardening.