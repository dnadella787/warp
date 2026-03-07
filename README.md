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

### Format code
```bash
clang-format -i $(git ls-files '*.cpp' '*.hpp')
```
Requires a recent `clang-format` (15 or newer recommended). The command rewrites all tracked C++ sources in place using the repository’s `.clang-format` configuration (tab-indented, multi-line enum entries).

### TODO 
- robust Router
- metrics 
- (m)TLS support
- Request/response interceptors
- Return output as JSON
- Tag with request-id in header
- Lots of unit tests (with GoogleTest)
- Code generation from YAML (using Jinja?) and allow registration of classes
- Add benchmark suite (Google Benchmark) and fuzz targets for parser hardening.
