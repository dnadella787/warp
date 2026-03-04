# Warp HTTP Framework

Warp is a high-performance HTTP framework that exposes a modern, Boost-free public API while leveraging Boost.Beast and Boost.Asio internally for low-level networking.

## Highlights
- Modern C++20 header-based façade (`warp/http/server.hpp`) that keeps Boost types internal.
- Coroutine-friendly request handlers with value-oriented `request` and `response` wrappers.
- Internal use of Boost.Asio for I/O multiplexing and Boost.Beast for HTTP parsing and serialization.
- Route registry with thread-safe updates and zero-copy response paths where possible.
- Pattern-based routing with `{param}` segments surfaced through `request::path_param()`.

> **Prerequisites:** CMake 3.20+, a C++20-capable compiler (Clang 15+, GCC 11+, or MSVC 17.7+), and Boost headers (Asio + Beast) available on the include path.

## Getting Started
```bash
cmake -S . -B build
cmake --build build --config Release
./build/examples/warp_example_server
```

The server example blocks inside `server.run()`; press `Ctrl+C` in the terminal to stop it. With the server running, you can ping it from another terminal using the request helper:

```bash
./build/examples/warp_example_request
```

## Next Steps
- Flesh out middleware, metrics, and TLS support.
- Add benchmark suite (Google Benchmark) and fuzz targets for parser hardening.
- Document threading model and lifecycle hooks in depth.
