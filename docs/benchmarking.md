# Benchmarking

Warp includes a Google Benchmark target for comparing the callback-based event loop with the coroutine-based event loop.

## Benchmark Target

Enable benchmarks at configure time:

```bash
cmake -S . -B build-bench \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release \
  -Dwarp_BUILD_BENCHMARKS=ON \
  -Dwarp_BUILD_TESTS=OFF \
  -Dwarp_BUILD_EXAMPLES=OFF
```

Build the benchmark:

```bash
cmake --build build-bench --target warp_http_event_loop_benchmark -j4
```

Run it:

```bash
./build-bench/benchmarks/warp_http_event_loop_benchmark \
  --benchmark_min_time=1.5s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true
```

The benchmark source is [http_event_loop_benchmark.cpp](/Users/dnadella/Projects/warp/benchmarks/http_event_loop_benchmark.cpp).

## What It Measures

The benchmark starts a real Warp server on `127.0.0.1` and uses a real TCP client over a keep-alive connection.

Each benchmark iteration:

1. Sends `GET /ping`
2. Reads the full HTTP response
3. Measures real wall-clock round-trip time

The route handler is intentionally simple so the comparison stays focused on the event-loop implementation rather than application logic.

## Results

Measured on this machine with:

- `arm64`
- `Release` build
- 5 repetitions
- aggregate-only Google Benchmark reporting

Observed aggregates:

| Mode | Mean round-trip time | Median round-trip time | Mean throughput | Median throughput |
| --- | ---: | ---: | ---: | ---: |
| Callbacks | 77.0 us | 77.2 us | 12.98k req/s | 12.96k req/s |
| Coroutines | 83.2 us | 82.7 us | 12.02k req/s | 12.10k req/s |

In this run, the callback event loop was about 8% faster on this minimal round-trip benchmark.

The exact Google Benchmark aggregates captured were:

```text
BM_CallbackEventLoop_RoundTrip/0/min_time:1.000/real_time_mean    77.0 us   bytes_per_second=798.642Ki/s items_per_second=12.9811k/s
BM_CallbackEventLoop_RoundTrip/0/min_time:1.000/real_time_median  77.2 us   bytes_per_second=797.218Ki/s items_per_second=12.958k/s
BM_CallbackEventLoop_RoundTrip/0/min_time:1.000/real_time_stddev 0.392 us

BM_CoroutineEventLoop_RoundTrip/1/min_time:1.000/real_time_mean   83.2 us   bytes_per_second=739.587Ki/s items_per_second=12.0212k/s
BM_CoroutineEventLoop_RoundTrip/1/min_time:1.000/real_time_median 82.7 us   bytes_per_second=744.21Ki/s items_per_second=12.0964k/s
BM_CoroutineEventLoop_RoundTrip/1/min_time:1.000/real_time_stddev 2.56 us
```

## Interpretation

This benchmark measures the overhead of the HTTP event loop itself under a very small handler.

- The callback path is currently the faster default for raw round-trip latency.
- The coroutine path remains close, but adds a small amount of scheduler/state-machine overhead in this scenario.
- These numbers are directional, not universal. If your handlers spend most of their time waiting on database or network I/O, the event-loop overhead becomes a much smaller part of total request time.

## Notes

- The benchmark uses real sockets on localhost, so it needs an environment where binding local ports is allowed.
- Google Benchmark on this macOS setup could not determine CPU frequency metadata correctly. That warning does not affect the measured wall-clock results above.
- Re-run the benchmark on an otherwise idle machine if you want cleaner absolute numbers.
