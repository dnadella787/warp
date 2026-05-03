# Benchmarking

Warp includes a Google Benchmark target for comparing callback and coroutine event-loop overhead under sustained concurrent load, with both plain HTTP and TLS `/ping` scenarios.

## Benchmark Results

Sample run captured on `2026-05-02` on a 2023 MacBook Pro (M3, 36 GB RAM, 12 CPU cores).

Run with:
```bash
./build/benchmarks/warp_http_event_loop_benchmark \
  --warp-benchmark-concurrency=500 \
  --warp-benchmark-client-threads=8 \
  --warp-benchmark-warmup=15s \
  --warp-benchmark-duration=60s \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_counters_tabular=true
```

Google Benchmark on this macOS setup still emits:

```text
Unable to determine clock rate from sysctl: hw.cpufrequency: No such file or directory
This does not affect benchmark measurements, only the metadata output.
***WARNING*** Failed to set thread affinity. Estimated CPU frequency may be incorrect.
```

These warnings affect benchmark metadata, not the measured wall-clock results.

### Round-trip throughput

| Event Loop | TLS | Mean RPS   | Median RPS | Mean p99 latency |
|------------|-----|------------|------------|------------------|
| Callback   | Off | 196.920k/s | 196.864k/s | 2.981 ms         |
| Callback   | On  | 174.317k/s | 172.878k/s | 3.845 ms         |
| Coroutine  | Off | 163.552k/s | 162.560k/s | 3.543 ms         |
| Coroutine  | On  | 166.061k/s | 166.988k/s | 3.746 ms         |

### DB-backed throughput

| Event Loop | Mean RPS   | Median RPS | Mean p99 latency |
|------------|------------|------------|------------------|
| Callback   | 5.32462k/s | 5.31945k/s | 100.254 ms       |
| Coroutine  | 5.33959k/s | 5.32850k/s | 98.479 ms        |

All six `manual_time_mean` rows held `steady_concurrency_pct=100` with zero connect, read, write, or HTTP status errors during the measured window.

### Example output excerpt

```text
Unable to determine clock rate from sysctl: hw.cpufrequency: No such file or directory
This does not affect benchmark measurements, only the metadata output.
***WARNING*** Failed to set thread affinity. Estimated CPU frequency may be incorrect.
2026-05-02T23:44:12-05:00
Running ./build/benchmarks/warp_http_event_loop_benchmark
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 1.96, 3.92, 5.45
warp_benchmark_client_threads: 8
warp_benchmark_concurrency_levels: 500
warp_benchmark_connect_timeout: 5s
warp_benchmark_duration: 60s
warp_benchmark_request_timeout: 5s
warp_benchmark_warmup: 15s
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                   Time             CPU   Iterations bytes_per_second completed_responses connect_errors connected_max connected_min error_rate_pct failed_requests items_per_second latency_max_us latency_p50_us latency_p90_us latency_p99_us latency_samples_kept latency_samples_observed proc_cpu_pct proc_cpu_us_per_req read_errors response_status_errors rss_peak_mib steady_concurrency_pct successful_requests target_concurrency throughput_req_per_s write_errors
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
BM_CallbackEventLoop_RoundTrip/concurrency:500/tls:off/iterations:1/manual_time_mean         60003 ms          344 ms            3      11.8307Mi/s            11.8152M              0           500           500              0               0        196.91k/s       8.20533k         2.569k       2.72767k       2.98067k             11.8152M                 11.8152M      670.459             34.0499           0                      0      205.219                    100            11.8152M                500              196.92k            0 concurrency_levels=500, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/tls:on/iterations:1/manual_time_mean          60000 ms          334 ms            3      10.4732Mi/s             10.459M              0           500           500              0               0       174.317k/s       53.6683k       2.86167k       3.11167k       3.84467k              10.459M                  10.459M      684.226             39.2553           0                      0      408.125                    100             10.459M                500             174.317k            0 concurrency_levels=500, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/tls:off/iterations:1/manual_time_mean        60001 ms          296 ms            3      9.82623Mi/s            9.81312M              0           500           500              0               0       163.548k/s         8.804k       3.06567k       3.24767k         3.543k             9.81312M                 9.81312M      658.631             40.2688           0                      0      409.109                    100            9.81312M                500             163.552k            0 concurrency_levels=500, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/tls:on/iterations:1/manual_time_mean         60000 ms          312 ms            3      9.97713Mi/s            9.96366M              0           500           500              0               0        166.06k/s        24.736k       2.99367k       3.24367k         3.746k             9.96366M                 9.96366M      649.459             39.1278           0                      0      463.318                    100            9.96366M                500             166.061k            0 concurrency_levels=500, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_mean               60000 ms         55.2 ms            3      395.186Ki/s            319.477k              0           500           500              0               0       5.32462k/s       105.888k        94.274k       95.6657k       100.254k             319.477k                 319.477k      50.0927              94.077           0                      0      429.052                    100            319.477k                500             5.32462k            0 concurrency_levels=500, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_mean              60000 ms         54.6 ms            3      396.298Ki/s            320.375k              0           500           500              0               0       5.33959k/s       105.285k         93.97k       95.0147k        98.479k             320.375k                 320.375k      52.5771             98.4678           0                      0       431.74                    100            320.375k                500             5.33959k            0 concurrency_levels=500, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s

```

You may notice errors like:
```text
ERROR OCCURRED: 'timed out before benchmark clients reached target concurrency (connected=294/500, connect_attempts=18147, connect_failures=0)'
```
in the synchronous DB benchmark variant. This happens because the blocking query runs on the main I/O threads, which drags down throughput and can prevent the client from reaching target concurrency.

Increasing the client-side connect timeout (`--warp-benchmark-connect-timeout=10s`) can keep the run from failing early, but the result is still instructive: once load grows, async DB queries preserve concurrency better than forcing clients to wait longer for blocked event-loop threads.

The benchmark binary contains:

- a minimal in-memory `GET /ping` round trip over plain HTTP (`/tls:off`)
- the same `GET /ping` round trip with TLS enabled (`/tls:on`)
- when `warp_BUILD_DB=ON`, a PostgreSQL-backed `GET /db/exchanges/nyse` round trip that queries `exchanges` for `exchange_code = 'NYSE'`

## Benchmark Target

Enable benchmarks at configure time:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -Dwarp_BUILD_DB=ON \
  -Dwarp_BUILD_BENCHMARKS=ON \
  -Dwarp_BUILD_TESTS=OFF \
  -Dwarp_BUILD_EXAMPLES=OFF
```

Build the benchmark:

```bash
cmake --build build --target warp_http_event_loop_benchmark -j4
```

If you are on Apple Silicon and your shell is running under Rosetta, use `arch -arm64` for configure, build, and run steps so the benchmark links against the native Homebrew libraries.

## Running The Load Test

Example:
`warp-benchmark-concurrency` is technically just the number of client side sessions spawned but each client side session creates a corresponding server side session too. You will end up creating 2 * `warp-benchmark-concurrency` sessions and using the same number of socket file descriptors. Check your system limits accordingly or the listener crash on occasion trying to create new sockets for incoming requests.

```bash
./build/benchmarks/warp_http_event_loop_benchmark \
  --warp-benchmark-concurrency=500 \
  --warp-benchmark-client-threads=8 \
  --warp-benchmark-warmup=15s \
  --warp-benchmark-duration=60s \
  --benchmark_repetitions=3 \
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
- `--warp-benchmark-warmup=15s`
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

For a local Dockerized PostgreSQL instance plus seeded `exchanges` data, use [postgres-local.md](postgres-local.md) or run:

```bash
./scripts/init_local_postgres.sh
```

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
- Google Benchmark on this macOS setup could not determine CPU frequency metadata correctly and also failed to set thread affinity. Those warnings do not affect the measured wall-clock results.
- Re-run the benchmark on an otherwise idle machine if you want cleaner absolute numbers.
