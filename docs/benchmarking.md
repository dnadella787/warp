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
| Callback   | 500             | 162.24 k/s     | 2.29 k/s          |
| Callback   | 1000            | 157.65 k/s     | 2.30 k/s          |
| Coroutine  | 500             | 151.55 k/s     | 2.35 k/s          |
| Coroutine  | 1000            | 149.97 k/s     | 2.27 k/s          |

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
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 4.43, 2.68, 2.64
warp_benchmark_client_threads: 8
warp_benchmark_concurrency_levels: 500,1000
warp_benchmark_connect_timeout: 5s
warp_benchmark_duration: 60s
warp_benchmark_request_timeout: 5s
warp_benchmark_warmup: 15s
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                   Time             CPU   Iterations bytes_per_second completed_responses connect_errors connected_max connected_min error_rate_pct failed_requests items_per_second latency_max_us latency_p50_us latency_p90_us latency_p99_us latency_samples_kept latency_samples_observed proc_cpu_pct proc_cpu_us_per_req read_errors response_status_errors rss_peak_mib steady_concurrency_pct successful_requests target_concurrency throughput_req_per_s write_errors
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_mean            60001 ms          112 ms            5      9.74762Mi/s            9.73454M              0           500           500              0               0        162.24k/s       12.7776k        3.0814k        3.2716k         4.136k             9.73454M                 9.73454M      609.241             37.5523           0                      0      232.297                    100            9.73454M                500             162.242k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_median          60000 ms          112 ms            5      9.71578Mi/s             9.7026M              0           500           500              0               0        161.71k/s        13.322k         3.073k         3.283k         3.665k              9.7026M                  9.7026M      609.336             37.6907           0                      0      263.203                    100             9.7026M                500              161.71k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_stddev           1.78 ms         1.95 ms            5      67.4046Ki/s            65.6659k              0             0             0              0               0       1.09559k/s       6.44992k        21.8014        21.7899        834.638             65.6659k                 65.6659k      8.58741            0.507033           0                      0      70.7687                      0            65.6659k                  0             1.09443k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_cv               0.00 %          1.75 %             5            0.68%               0.67%          0.00%         0.00%         0.00%          0.00%           0.00%            0.68%         50.48%          0.71%          0.67%         20.18%                0.67%                    0.67%        1.41%               1.35%       0.00%                  0.00%       30.46%                  0.00%               0.67%              0.00%                0.67%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_mean           60000 ms          107 ms            5      9.10301Mi/s            9.09071M              0           500           500              0               0       151.511k/s       11.5502k        3.2692k        3.6538k        4.4448k             9.09071M                 9.09071M      601.602             39.7226           0                      0      345.491                    100            9.09071M                500             151.512k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_median         60000 ms          107 ms            5      9.10922Mi/s            9.09686M              0           500           500              0               0       151.614k/s        13.268k         3.264k         3.634k         3.995k             9.09686M                 9.09686M      604.405             39.6184           0                      0      348.891                    100            9.09686M                500             151.614k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_stddev         0.461 ms         2.43 ms            5      262.358Ki/s            255.811k              0             0             0              0               0       4.26436k/s       3.16756k        66.9604        144.697        896.641             255.811k                 255.811k      7.63078            0.748548           0                      0      22.7288                      0            255.811k                  0             4.26352k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:500/iterations:1/manual_time_cv              0.00 %          2.27 %             5            2.81%               2.81%          0.00%         0.00%         0.00%          0.00%           0.00%            2.81%         27.42%          2.05%          3.96%         20.17%                2.81%                    2.81%        1.27%               1.88%       0.00%                  0.00%        6.58%                  0.00%               2.81%              0.00%                2.81%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_mean           60000 ms          112 ms            5      9.47198Mi/s            9.45915M              0            1k            1k              0               0       157.652k/s       16.5184k        6.3748k        6.6156k         7.503k             9.45915M                 9.45915M      629.763             39.9451           0                      0      379.291                    100            9.45915M                 1k             157.652k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_median         60000 ms          112 ms            5      9.46957Mi/s            9.45673M              0            1k            1k              0               0       157.612k/s        17.373k         6.371k         6.613k         7.548k             9.45673M                 9.45673M      629.364             39.8849           0                      0      380.812                    100            9.45673M                 1k             157.612k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_stddev         0.099 ms        0.708 ms            5      47.7117Ki/s            46.5171k              0             0             0              0               0        775.504/s       2.40971k        34.3613        36.0111        180.876             46.5171k                 46.5171k      8.37938              0.3503           0                      0      6.81408                      0            46.5171k                  0              775.284            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_cv              0.00 %          0.63 %             5            0.49%               0.49%          0.00%         0.00%         0.00%          0.00%           0.00%            0.49%         14.59%          0.54%          0.54%          2.41%                0.49%                    0.49%        1.33%               0.88%       0.00%                  0.00%        1.80%                  0.00%               0.49%              0.00%                0.49%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_mean          60003 ms          112 ms            5      9.01061Mi/s            8.99884M              0            1k            1k              0               0       149.973k/s        29.203k         6.627k        7.0832k        8.2862k             8.99884M                 8.99884M       597.06             39.8165           0                      0      397.881                    100            8.99884M                 1k             149.981k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_median        60002 ms          113 ms            5      8.99928Mi/s            8.98772M              0            1k            1k              0               0       149.785k/s        26.169k          6.61k         7.124k         7.696k             8.98772M                 8.98772M      595.518             39.9678           0                      0      397.984                    100            8.98772M                 1k             149.795k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_stddev         3.68 ms         4.05 ms            5      121.242Ki/s            118.393k              0             0             0              0               0       1.97067k/s       15.9109k        83.3607        134.353       1.04761k             118.393k                 118.393k      7.24629            0.702834           0                      0      2.49823                      0            118.393k                  0             1.97322k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_RoundTrip/concurrency:1000/iterations:1/manual_time_cv             0.01 %          3.60 %             5            1.31%               1.32%          0.00%         0.00%         0.00%          0.00%           0.00%            1.31%         54.48%          1.26%          1.90%         12.64%                1.32%                    1.32%        1.21%               1.77%       0.00%                  0.00%        0.63%                  0.00%               1.32%              0.00%                1.32%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_mean          60002 ms         26.9 ms            5      170.249Ki/s            137.637k              0           500           500              0               0       2.29388k/s       302.245k       217.947k        237.85k       267.236k             137.637k                 137.637k       90.203             396.074           0                      0      402.431                    100            137.637k                500             2.29395k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_median        60002 ms         26.9 ms            5       166.06Ki/s            134.251k              0           500           500              0               0       2.23745k/s       300.093k       222.029k       238.146k       268.988k             134.251k                 134.251k      96.8376             440.207           0                      0      402.641                    100            134.251k                500             2.23752k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_stddev         1.25 ms        0.847 ms            5      7.92056Ki/s            6.40286k              0             0             0              0               0        106.719/s        11.789k       9.03824k       3.99216k        5.5199k             6.40286k                 6.40286k      14.4263              78.384           0                      0     0.514985                      0            6.40286k                  0              106.714            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_cv             0.00 %          3.15 %             5            4.65%               4.65%          0.00%         0.00%         0.00%          0.00%           0.00%            4.65%          3.90%          4.15%          1.68%          2.07%                4.65%                    4.65%       15.99%              19.79%       0.00%                  0.00%        0.13%                  0.00%               4.65%              0.00%                4.65%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_mean         60003 ms         29.8 ms            5      174.446Ki/s            141.032k              0           500           500              0               0       2.35042k/s       298.797k       214.289k       235.978k       265.845k             141.032k                 141.032k      96.0037             413.837           0                      0      405.588                    100            141.032k                500             2.35053k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_median       60002 ms         28.8 ms            5      171.261Ki/s            138.456k              0           500           500              0               0       2.30751k/s       291.917k       217.111k       235.736k       266.959k             138.456k                 138.456k      103.773             449.719           0                      0      405.688                    100            138.456k                500              2.3076k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_stddev        1.53 ms         4.09 ms            5      10.6252Ki/s            8.59166k              0             0             0              0               0        143.161/s       20.4842k       13.4128k       3.99849k         7.134k             8.59166k                 8.59166k      20.5414             111.169           0                      0     0.835945                      0            8.59166k                  0              143.194            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:500/iterations:1/manual_time_cv            0.00 %         13.70 %             5            6.09%               6.09%          0.00%         0.00%         0.00%          0.00%           0.00%            6.09%          6.86%          6.26%          1.69%          2.68%                6.09%                    6.09%       21.40%              26.86%       0.00%                  0.00%        0.21%                  0.00%               6.09%              0.00%                6.09%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_mean         60001 ms         28.3 ms            5       170.53Ki/s            137.862k              0            1k            1k              0               0       2.29767k/s       516.278k       435.625k       464.584k        491.59k             137.862k                 137.862k      96.0874              419.49           0                      0      408.691                    100            137.862k                 1k             2.29769k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_median       60001 ms         27.8 ms            5      169.044Ki/s             136.66k              0            1k            1k              0               0       2.27764k/s       518.639k       437.764k       467.801k       491.574k              136.66k                  136.66k      101.197             442.342           0                      0      408.672                    100             136.66k                 1k             2.27767k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_stddev       0.513 ms         3.53 ms            5       5.2072Ki/s            4.20859k              0             0             0              0               0        70.1602/s       9.58343k       7.44354k       9.78549k        5.3811k             4.20859k                 4.20859k      10.5801              55.864           0                      0     0.730993                      0            4.20859k                  0              70.1432            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CallbackEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_cv            0.00 %         12.48 %             5            3.05%               3.05%          0.00%         0.00%         0.00%          0.00%           0.00%            3.05%          1.86%          1.71%          2.11%          1.09%                3.05%                    3.05%       11.01%              13.32%       0.00%                  0.00%        0.18%                  0.00%               3.05%              0.00%                3.05%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_mean        60002 ms         27.6 ms            5      168.897Ki/s            136.545k              0            1k            1k              0               0       2.27567k/s       564.405k        436.21k       468.609k       501.255k             136.545k                 136.545k      102.338             449.867           0                      0      413.459                    100            136.545k                 1k             2.27575k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_median      60003 ms         27.6 ms            5      168.485Ki/s            136.207k              0            1k            1k              0               0       2.27012k/s       529.163k       435.546k       471.681k       499.274k             136.207k                 136.207k      99.3526             435.046           0                      0      413.141                    100            136.207k                 1k             2.27012k            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_stddev       2.02 ms         3.87 ms            5      1.70038Ki/s            1.37614k              0             0             0              0               0        22.9104/s       85.1455k       3.96959k       10.1814k       6.42315k             1.37614k                 1.37614k      4.90957             25.0409           0                      0      0.74956                      0            1.37614k                  0              22.9357            0 concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
BM_CoroutineEventLoop_DbRoundTrip/concurrency:1000/iterations:1/manual_time_cv           0.00 %         14.04 %             5            1.01%               1.01%          0.00%         0.00%         0.00%          0.00%           0.00%            1.01%         15.09%          0.91%          2.17%          1.28%                1.01%                    1.01%        4.80%               5.57%       0.00%                  0.00%        0.18%                  0.00%               1.01%              0.00%                1.01%        0.00% concurrency_levels=500,1000, client_threads=8, warmup=15s, duration=60s, connect_timeout=5s, request_timeout=5s
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
