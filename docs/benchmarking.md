# Benchmarking

Warp includes a Google Benchmark target for comparing the callback-based event loop with the coroutine-based event loop under sustained concurrent load.

The benchmark binary contains:

- a minimal in-memory `GET /ping` round trip
- when `warp_BUILD_DB=ON`, a PostgreSQL-backed `GET /db/exchanges/nyse` round trip that queries `exchanges` for `exchange_code = 'NYSE'`

## Benchmark Target

Enable benchmarks at configure time:

```bash
cmake -S . -B build-bench \
  -DCMAKE_BUILD_TYPE=Release \
  -Dwarp_BUILD_DB=ON \
  -Dwarp_BUILD_BENCHMARKS=ON \
  -Dwarp_BUILD_TESTS=OFF \
  -Dwarp_BUILD_EXAMPLES=OFF
```

Build the benchmark:

```bash
cmake --build build-bench --target warp_http_event_loop_benchmark -j4
```

If you are on Apple Silicon and your shell is running under Rosetta, use `arch -arm64` for configure, build, and run steps so the benchmark links against the native Homebrew libraries.

## Running The Load Test

Example:

```bash
./build-bench/benchmarks/warp_http_event_loop_benchmark \
  --warp-benchmark-concurrency=1k,5k,10k \
  --warp-benchmark-client-threads=8 \
  --warp-benchmark-warmup=5s \
  --warp-benchmark-duration=60s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true \
  --benchmark_counters_tabular=true
```

Useful options:

- `--warp-benchmark-concurrency=...`
  - Single value: `1024`
  - List: `1k,5k,10k`
  - Range: `1k-5k:1k`
- `--warp-benchmark-client-threads=8`
  - Number of async load-generator shards. Keep this low enough to avoid client-side scheduling overhead.
- `--warp-benchmark-warmup=5s`
  - Warm-up interval before measurements start.
- `--warp-benchmark-duration=60s`
  - Measured interval for each repetition.
- `--warp-benchmark-connect-timeout=5s`
  - Connection establishment timeout.
- `--warp-benchmark-request-timeout=5s`
  - Per-request read/write timeout.

The shorter `--warp-bench-*` aliases are also accepted.

The same settings can also be supplied through environment variables:

- `WARP_BENCH_CONCURRENCY`
- `WARP_BENCH_CLIENT_THREADS`
- `WARP_BENCH_WARMUP`
- `WARP_BENCH_DURATION`
- `WARP_BENCH_CONNECT_TIMEOUT`
- `WARP_BENCH_REQUEST_TIMEOUT`

If you configure with `warp_BUILD_DB=OFF`, the benchmark target still builds, but only the non-DB `/ping` scenario is compiled into the binary.

## PostgreSQL Benchmark Setup

The DB-backed benchmark uses the same environment variables as the example server and integration tests:

```bash
export WARP_DB_HOST=127.0.0.1
export WARP_DB_PORT=5432
export WARP_DB_USER=...
export WARP_DB_PASSWORD=...
export WARP_DB_NAME=...
```

Only `WARP_DB_USER`, `WARP_DB_PASSWORD`, and `WARP_DB_NAME` are required. If they are not set, the DB benchmark entries are skipped.

## Methodology

The benchmark keeps the original intent of comparing Warp event-loop overhead with a real TCP client and a real in-process Warp server on `127.0.0.1`, but it now drives the server with a high-concurrency constant-concurrency load instead of one serialized client connection.

Each virtual client:

1. Opens a keep-alive TCP connection to the benchmark server.
2. Keeps at most one request in flight on that connection.
3. Immediately sends the next request after the previous response completes.
4. Continues until the measured duration ends.

The load generator is asynchronous and sharded across a configurable number of client threads so the benchmark can sustain thousands of concurrent clients without the benchmark client becoming a single-threaded bottleneck.

Each benchmark registration runs one warm-up phase plus one measured phase. Use Google Benchmark repetitions to collect multiple comparable runs with the same load shape.

## Metrics

Each run reports:

- `throughput_req_per_s`
  - Completed requests per second across the measured interval.
- `successful_requests`
  - Successful measured requests.
- `failed_requests`
  - Measured failures across connection, write, read, and HTTP status errors.
- `error_rate_pct`
  - `failed_requests / (successful_requests + failed_requests) * 100`.
- `latency_p50_us`, `latency_p90_us`, `latency_p99_us`, `latency_max_us`
  - End-to-end request latency for measured responses.
- `connect_errors`, `write_errors`, `read_errors`, `response_status_errors`
  - Failure breakdown.
- `connected_min`, `connected_max`, `steady_concurrency_pct`
  - Whether the benchmark actually held the requested concurrency during the measured window.
- `proc_cpu_us_per_req`, `proc_cpu_pct`, `rss_peak_mib`
  - Benchmark-process resource usage, including both the client load generator and the in-process Warp server.

Latency percentiles are computed from the exact measured latencies for each run. That maximizes accuracy, at the cost of keeping one latency sample per completed measured request in memory until the run finishes.

## Example Output

Short `/ping` validation run on this machine:

```text
Benchmark                                                                         Time             CPU   Iterations bytes_per_second completed_responses connect_errors connected_max connected_min error_rate_pct failed_requests items_per_second latency_max_us latency_p50_us latency_p90_us latency_p99_us latency_samples_kept latency_samples_observed proc_cpu_pct proc_cpu_us_per_req read_errors response_status_errors rss_peak_mib steady_concurrency_pct successful_requests target_concurrency throughput_req_per_s write_errors
BM_CallbackEventLoop_RoundTrip/concurrency:128/iterations:1/manual_time        2000 ms         2.76 ms            1      3.07012Mi/s            102.199k              0           128           128              0               0       51.0992k/s         5.495k         2.496k           2.6k         2.917k             102.199k                 102.199k      462.295               90.47           0                      0       11.375                    100            102.199k                128             51.0995k            0
BM_CoroutineEventLoop_RoundTrip/concurrency:128/iterations:1/manual_time       2005 ms         2.39 ms            1      3.07279Mi/s            102.518k              0           128           128              0               0       51.1437k/s          3.46k         2.494k         2.608k         2.754k             102.518k                 102.518k      478.986             93.6549           0                      0      12.9531                    100            102.518k                128              51.259k            0
```

This sample is only a sanity check. Re-run on an otherwise idle machine with longer durations and multiple repetitions before treating the numbers as representative.

## Design Notes

- Constant concurrency was chosen over constant request rate because it preserves the original round-trip comparison while removing the single-client serialization bottleneck.
- Each connection keeps one in-flight request instead of using deep HTTP pipelining. That avoids benchmarking a single socket's pipeline behavior instead of the server's ability to serve many concurrent clients.
- The benchmark registers a separate case per configured concurrency level, so `--benchmark_filter` can target specific scenarios and load levels after runtime argument parsing.
- `steady_concurrency_pct` should stay close to `100`. If it drops materially, the server or the client load generator could not sustain the requested concurrency for the full measured interval.

## Notes

- The benchmark uses real localhost sockets, so it must run in an environment where binding local ports is allowed.
- Very high concurrency values can exceed the process file-descriptor limit. If connection setup fails before the run starts, raise `ulimit -n`.
- The DB-backed benchmark also requires a reachable PostgreSQL instance with an `exchanges` table and a row for `exchange_code = 'NYSE'` if you want a successful `200 OK` response.
- Google Benchmark on this macOS setup could not determine CPU frequency metadata correctly. That warning does not affect the measured wall-clock results.
- Re-run the benchmark on an otherwise idle machine if you want cleaner absolute numbers.
