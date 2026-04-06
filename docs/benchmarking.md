# Benchmarking

## Benchmark Results
Run on 2023 M3 Macbook pro 36GB RAM 12 core

### RPS Results

Run with:
```c++
warp_benchmark_client_threads: 8
warp_benchmark_concurrency_levels: 500, 1000
warp_benchmark_connect_timeout: 5s
warp_benchmark_duration: 60s
warp_benchmark_request_timeout: 5s
warp_benchmark_warmup: 15s
```

| Mode       | Client Sessions | /ping Mean RPS | /db/NYSE Mean RPS |
|------------|-----------------|----------------|-------------------|
| Callback   | 500             | 202.879 k/s    | 2.72376k/s        |
| Callback   | 1000            | 204.347k/s     | 2.66088k/s        |
| Coroutine  | 500             | 174.205k/s     | 2.67621k/s        |
| Coroutine  | 1000            | 178.731k/s     | 2.63886k/s        |

### Latency results

Run with:
```c++
warp_benchmark_client_threads: 4
warp_benchmark_concurrency_levels: 12
warp_benchmark_connect_timeout: 5s
warp_benchmark_duration: 30s
warp_benchmark_request_timeout: 5s
warp_benchmark_warmup: 5s
```

| Mode       | /ping p99 latency (us) | /db/NYSE p99 latency (ms) |
|------------|------------------------|---------------------------|
| Callback   | 139.8                  | 8.04                      |
| Coroutine  | 146.4                  | 7.73                      |

### Example output:

```text
Unable to determine clock rate from sysctl: hw.cpufrequency: No such file or directory
This does not affect benchmark measurements, only the metadata output.
***WARNING*** Failed to set thread affinity. Estimated CPU frequency may be incorrect.
2026-04-05T21:45:40-05:00
Running ./build/benchmarks/warp_http_event_loop_benchmark
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 3.52, 3.53, 5.02
warp_benchmark_client_threads: 8
warp_benchmark_concurrency_levels: 500,1000
warp_benchmark_connect_timeout: 5s
warp_benchmark_duration: 60s
warp_benchmark_request_timeout: 5s
warp_benchmark_warmup: 15s
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                   Time             CPU   Iterations bytes_per_second completed_responses connect_errors connected_max connected_min error_rate_pct failed_requests items_per_second latency_max_us latency_p50_us latency_p90_us latency_p99_us latency_samples_kept latency_samples_observed proc_cpu_pct proc_cpu_us_per_req read_errors response_status_errors rss_peak_mib steady_concurrency_pct successful_requests target_concurrency throughput_req_per_s write_errors
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_mean            60000 ms          312 ms            5      12.1893Mi/s            12.1728M              0           500           500              0               0       202.879k/s        6.7698k        2.4972k        2.6412k         2.845k             12.1728M                 12.1728M      664.532             32.7554           0                      0      220.294                    100            12.1728M                500              202.88k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_median          60000 ms          312 ms            5      12.1832Mi/s            12.1668M              0           500           500              0               0       202.779k/s         5.942k         2.499k         2.641k         2.816k             12.1668M                 12.1668M      665.449              32.822           0                      0      237.953                    100            12.1668M                500              202.78k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_stddev          0.563 ms         2.11 ms            5      30.0082Ki/s            29.3514k              0             0             0              0               0        487.753/s       2.60469k        7.62889        4.96991        63.9766             29.3514k                 29.3514k      1.94844             0.16509           0                      0      58.1092                      0            29.3514k                  0              489.189            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_cv               0.00 %          0.68 %             5            0.24%               0.24%          0.00%         0.00%         0.00%          0.00%           0.00%            0.24%         38.48%          0.31%          0.19%          2.25%                0.24%                    0.24%        0.29%               0.50%       0.00%                  0.00%       26.38%                  0.00%               0.24%              0.00%                0.24%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_mean           60002 ms          277 ms            5      10.4665Mi/s            10.4527M              0           500           500              0               0       174.205k/s        10.718k        2.8954k        3.1416k        3.4938k             10.4527M                 10.4527M      661.612             38.1664           0                      0      302.881                    100            10.4527M                500             174.211k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_median         60000 ms          278 ms            5      10.4008Mi/s             10.387M              0           500           500              0               0       173.112k/s         9.645k          2.93k         3.146k         3.462k              10.387M                  10.387M      657.811             37.9992           0                      0      301.734                    100             10.387M                500             173.116k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_stddev          3.76 ms         11.1 ms            5      698.957Ki/s            681.437k              0             0             0              0               0       11.3608k/s       4.60264k        211.151        114.723        259.748             681.437k                 681.437k      27.5031             3.76887           0                      0      13.1319                      0            681.437k                  0             11.3573k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_cv              0.01 %          3.99 %             5            6.52%               6.52%          0.00%         0.00%         0.00%          0.00%           0.00%            6.52%         42.94%          7.29%          3.65%          7.43%                6.52%                    6.52%        4.16%               9.87%       0.00%                  0.00%        4.34%                  0.00%               6.52%              0.00%                6.52%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_mean           60003 ms          320 ms            5      12.2775Mi/s            12.2615M              0            1k            1k              0               0       204.347k/s       12.0722k        4.9662k        5.1308k        5.3848k             12.2615M                 12.2615M      667.205             32.6509           0                      0      371.088                    100            12.2615M                 1k             204.358k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_median         60002 ms          320 ms            5      12.2718Mi/s            12.2555M              0            1k            1k              0               0       204.253k/s        12.236k         4.964k         5.117k         5.412k             12.2555M                 12.2555M      668.133             32.7176           0                      0      381.266                    100            12.2555M                 1k             204.259k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_stddev          3.28 ms         2.61 ms            5      48.3989Ki/s             47.593k              0             0             0              0               0        786.675/s       1.90872k        14.4291        41.5716        113.092              47.593k                  47.593k      2.34382             0.15997           0                      0      42.1864                      0             47.593k                  0              793.217            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_cv              0.01 %          0.82 %             5            0.38%               0.39%          0.00%         0.00%         0.00%          0.00%           0.00%            0.38%         15.81%          0.29%          0.81%          2.10%                0.39%                    0.39%        0.35%               0.49%       0.00%                  0.00%       11.37%                  0.00%               0.39%              0.00%                0.39%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_mean          60000 ms          287 ms            5      10.7384Mi/s            10.7239M              0            1k            1k              0               0       178.731k/s       13.9708k        5.6268k        5.8632k        6.3294k             10.7239M                 10.7239M      654.778             36.7391           0                      0      448.078                    100            10.7239M                 1k             178.732k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_median        60000 ms          286 ms            5      10.9119Mi/s            10.8971M              0            1k            1k              0               0       181.618k/s        12.969k         5.508k         5.724k         6.041k             10.8971M                 10.8971M      648.618             35.6445           0                      0      445.922                    100            10.8971M                 1k             181.619k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_stddev        0.234 ms         4.68 ms            5      536.346Ki/s            523.082k              0             0             0              0               0       8.71776k/s       2.37303k        339.258        378.224        614.146             523.082k                 523.082k       13.595             2.69017           0                      0      7.80008                      0            523.082k                  0             8.71804k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_cv             0.00 %          1.63 %             5            4.88%               4.88%          0.00%         0.00%         0.00%          0.00%           0.00%            4.88%         16.99%          6.03%          6.45%          9.70%                4.88%                    4.88%        2.08%               7.32%       0.00%                  0.00%        1.74%                  0.00%               4.88%              0.00%                4.88%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_mean          60004 ms         23.5 ms            5      202.154Ki/s            163.436k              0           500           500              0               0       2.72376k/s       295.896k       183.264k       201.072k       243.514k             163.436k                 163.436k      49.5336             182.085           0                      0      462.066                    100            163.436k                500             2.72393k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_median        60003 ms         25.5 ms            5      203.409Ki/s             164.44k              0           500           500              0               0       2.74067k/s       295.578k       183.306k       201.072k        236.17k              164.44k                  164.44k      49.1359             178.653           0                      0      462.125                    100             164.44k                500             2.74067k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_stddev         4.20 ms         3.75 ms            5      3.35785Ki/s            2.71706k              0             0             0              0               0        45.2427/s       15.5121k       4.78095k       2.70495k       18.7474k             2.71706k                 2.71706k      4.01194             17.6637           0                      0     0.256364                      0            2.71706k                  0              45.2844            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_cv             0.01 %         15.97 %             5            1.66%               1.66%          0.00%         0.00%         0.00%          0.00%           0.00%            1.66%          5.24%          2.61%          1.35%          7.70%                1.66%                    1.66%        8.10%               9.70%       0.00%                  0.00%        0.06%                  0.00%               1.66%              0.00%                1.66%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_mean         60001 ms         23.7 ms            5      198.625Ki/s            160.574k              0           500           500              0               0       2.67621k/s       313.911k       187.833k       202.795k       248.406k             160.574k                 160.574k       54.585             204.019           0                      0      464.444                    100            160.574k                500             2.67624k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_median       60000 ms         25.0 ms            5      199.058Ki/s            160.926k              0           500           500              0               0       2.68205k/s         306.5k       187.899k       202.222k       244.016k             160.926k                 160.926k      54.3723             205.734           0                      0      464.375                    100            160.926k                500              2.6821k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_stddev       0.642 ms         3.81 ms            5      2.10942Ki/s            1.70464k              0             0             0              0               0        28.4216/s       22.5871k       1.56713k       1.73107k       12.6293k             1.70464k                 1.70464k      1.76944             8.21019           0                      0      0.40724                      0            1.70464k                  0              28.4107            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_cv            0.00 %         16.10 %             5            1.06%               1.06%          0.00%         0.00%         0.00%          0.00%           0.00%            1.06%          7.20%          0.83%          0.85%          5.08%                1.06%                    1.06%        3.24%               4.02%       0.00%                  0.00%        0.09%                  0.00%               1.06%              0.00%                1.06%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_mean         60000 ms         24.6 ms            5      197.488Ki/s            159.654k              0            1k            1k              0               0       2.66088k/s       506.576k       375.994k       410.768k       465.096k             159.654k                 159.654k      50.9271             191.576           0                      0      471.878                    100            159.654k                 1k              2.6609k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_median       60000 ms         24.5 ms            5      196.328Ki/s            158.716k              0            1k            1k              0               0       2.64527k/s       504.628k        378.91k       413.085k       470.697k             158.716k                 158.716k      50.7975             192.032           0                      0      471.609                    100            158.716k                 1k             2.64527k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_stddev       0.442 ms         2.31 ms            5      2.74723Ki/s            2.22072k              0             0             0              0               0        37.0153/s       27.0091k       7.48914k       5.35079k       17.4986k             2.22072k                 2.22072k      3.85541             16.9581           0                      0     0.537123                      0            2.22072k                  0              37.0121            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_cv            0.00 %          9.36 %             5            1.39%               1.39%          0.00%         0.00%         0.00%          0.00%           0.00%            1.39%          5.33%          1.99%          1.30%          3.76%                1.39%                    1.39%        7.57%               8.85%       0.00%                  0.00%        0.11%                  0.00%               1.39%              0.00%                1.39%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_mean        60001 ms         25.5 ms            5      195.853Ki/s            158.334k              0            1k            1k              0               0       2.63886k/s       545.794k       376.719k       412.683k        482.31k             158.334k                 158.334k      53.1923             201.833           0                      0      476.531                    100            158.334k                 1k             2.63891k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_median      60000 ms         26.1 ms            5      194.199Ki/s            157.001k              0            1k            1k              0               0       2.61658k/s       519.413k       378.263k       417.767k       474.832k             157.001k                 157.001k      54.1784             207.058           0                      0      476.531                    100            157.001k                 1k             2.61668k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_stddev       1.48 ms         2.42 ms            5      3.59684Ki/s            2.90869k              0             0             0              0               0        48.4627/s       53.7702k       7.16948k       11.4341k       24.1127k             2.90869k                 2.90869k      3.82049             18.0276           0                      0     0.491381                      0            2.90869k                  0              48.4782            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_cv           0.00 %          9.50 %             5            1.84%               1.84%          0.00%         0.00%         0.00%          0.00%           0.00%            1.84%          9.85%          1.90%          2.77%          5.00%                1.84%                    1.84%        7.18%               8.93%       0.00%                  0.00%        0.10%                  0.00%               1.84%              0.00%                1.84%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s

```

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
`warp-benchmark-concurrency` is technically just the number of client side sessions spawned but each client side session creates a corresponding server side session too. You will end up creating 2 * `warp-benchmark-concurrency` sessions and using the same number of socket file descriptors. Check your system limits accordingly or the listener crash on occasion trying to create new sockets for incoming requests.

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
