# Benchmarking

Warp includes a Google Benchmark target for comparing the callback-based event loop with the coroutine-based event loop.

The benchmark binary contains:

- a minimal in-memory `GET /ping` round trip
- when `warp_BUILD_DB=ON`, a PostgreSQL-backed `GET /db/exchanges/nyse` round trip that queries `exchanges` for `exchange_code = 'NYSE'`

## Benchmark Target

Enable benchmarks at configure time:

```bash
cmake -S . -B build-bench \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
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

Run it:

```bash
./build-bench/benchmarks/warp_http_event_loop_benchmark \
  --benchmark_min_time=60s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true \
  --benchmark_counters_tabular=true
```

The benchmark sources live in [benchmarks/](/Users/dnadella/Projects/warp/benchmarks), with shared support code in
[http_event_loop_benchmark_support.cpp](/Users/dnadella/Projects/warp/benchmarks/http_event_loop_benchmark_support.cpp)
and separate scenario files for the in-memory and DB-backed cases.

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

## What It Measures

The benchmark starts a real Warp server on `127.0.0.1` and uses a real TCP client over a keep-alive connection.

Each benchmark iteration:

1. Sends either `GET /ping` or `GET /db/exchanges/nyse`
2. Reads the full HTTP response
3. Measures real wall-clock round-trip time

Each benchmark run also reports three resource counters for the Warp benchmark process itself:

- `proc_cpu_us_per_req`: total process CPU time divided by completed requests
- `proc_cpu_pct`: total process CPU time divided by wall-clock runtime
- `rss_peak_mib`: peak resident set size observed while the timed benchmark loop was running

These counters include the benchmark client and the in-process Warp server. The DB-backed benchmark does not include the external PostgreSQL server's own CPU or RAM usage.

The `/ping` route is intentionally simple so the comparison stays focused on the event-loop implementation rather than application logic.

The `/db/exchanges/nyse` route includes the PostgreSQL connection pool and one SQL query so you can see how the two event-loop modes behave when the handler suspends on database I/O.

## Results

### Minimal `/ping` Round Trip

Measured on this machine with:
- 2021 M1 Macbook Pro 32 GB  
- `arm64`
- `Release` build
- `--benchmark_min_time=60s`
- 5 repetitions
- aggregate-only Google Benchmark reporting

Observed aggregates:

| Mode | Mean round-trip time | Median round-trip time | Mean throughput | Median throughput | Mean proc CPU / req | Peak RSS |
| --- |---------------------:|-----------------------:|----------------:|------------------:|--------------------:|---------:|
| Callbacks |              77.9 us |                77.3 us |    12.84k req/s |      12.94k req/s |            76.33 us | 8.45 MiB |
| Coroutines |              89.8 us |                79.6 us |    12.54k req/s |      12.56k req/s |             78.1 us | 8.69 MiB |

In this run, the callback event loop remained slightly faster on raw `/ping` round-trip latency, but the gap was under 1%.

The exact Google Benchmark aggregates captured were:

### PostgreSQL `NYSE` Round Trip

Measured with the same machine and build tree, plus:

- `WARP_DB_HOST=localhost`
- `WARP_DB_PORT=5432`
- `WARP_DB_USER=localdbusr`
- `WARP_DB_PASSWORD=localdbpwd`
- `WARP_DB_NAME=api-db`

Observed aggregates for `GET /db/exchanges/nyse`:

| Mode | Mean round-trip time | Median round-trip time | Mean throughput | Median throughput | Mean proc CPU / req |  Peak RSS |
| --- |---------------------:|-----------------------:|----------------:|------------------:|--------------------:|----------:|
| Callbacks |              2135 us |                2136 us |    468.49 req/s |      468.14 req/s |            187.5 us | 19.38 MiB |
| Coroutines |              2308 us |                2195 us |    437.61 req/s |      455.55 req/s |            206.74 us | 19.45 MiB |

In this run, the coroutine event loop was also slower than the callbacks by about 8%. 

As your request handlers perform more I/O and make greater use of coroutines, the cost of the initial heap allocation for the coroutine stack becomes increasingly amortized.

### Exact Results
The exact Google Benchmark output captured was:

```text
Run on (8 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x8)
Load Average: 3.70, 6.97, 8.23
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                     Time             CPU   Iterations bytes_per_second items_per_second proc_cpu_pct proc_cpu_us_per_req rss_peak_mib
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_mean            77.9 us         19.5 us            5      790.235Ki/s       12.8445k/s      98.0379             76.3336      8.45312
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_median          77.3 us         19.5 us            5      795.897Ki/s       12.9365k/s      97.4233             76.5594      8.46875
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_stddev          1.00 us        0.200 us            5      10.0788Ki/s        163.821/s       1.1295             1.02539    0.0455543
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_cv              1.29 %          1.03 %             5            1.28%            1.28%        1.15%               1.34%        0.54%
BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_mean           79.8 us         19.0 us            5      771.443Ki/s        12.539k/s       97.926             78.0985      8.69063
BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_median         79.6 us         18.9 us            5      772.566Ki/s       12.5573k/s      97.9283             78.0155       8.6875
BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_stddev        0.351 us        0.073 us            5      3.38785Ki/s         55.066/s     0.297282            0.471207     6.98771m
BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_cv             0.44 %          0.38 %             5            0.44%            0.44%        0.30%               0.60%        0.08%
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_mean          2135 us         25.1 us            5      34.7709Ki/s        468.493/s      8.78429             187.505      19.3813
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_median        2136 us         25.1 us            5      34.7445Ki/s        468.137/s      8.79303             186.675       19.375
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_stddev        16.3 us        0.156 us            5        272.508/s        3.58563/s    0.0991743             2.06627    0.0178152
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_cv            0.76 %          0.62 %             5            0.77%            0.77%        1.13%               1.10%        0.09%
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_mean         2308 us         25.8 us            5      32.4788Ki/s        437.609/s      8.98274              206.74      19.3531
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_median       2195 us         25.5 us            5        33.81Ki/s        455.546/s       9.0811             202.509      19.4531
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_stddev        276 us         1.35 us            5      3.39482Ki/s        45.7408/s     0.287168             17.6833     0.223607
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_cv          11.94 %          5.22 %             5           10.45%           10.45%        3.20%               8.55%        1.16%
```

## Interpretation

This benchmark measures the overhead of the HTTP event loop itself under a very small handler.

- The callback path is currently the faster default for raw round-trip latency.
- The coroutine path is now very close on the minimal `/ping` benchmark and used slightly less process CPU time per request in this run.
- These numbers are directional, not universal. The DB-backed benchmark shows that once the handler spends most of its time waiting on PostgreSQL, event-loop overhead becomes a much smaller part of total request time.

## Notes

- The benchmark uses real sockets on localhost, so it needs an environment where binding local ports is allowed.
- The DB-backed benchmark also requires a reachable PostgreSQL instance with an `exchanges` table and a row for `exchange_code = 'NYSE'` if you want a successful `200 OK` response.
- Google Benchmark on this macOS setup could not determine CPU frequency metadata correctly. That warning does not affect the measured wall-clock results above.
- Google Benchmark rejected `--benchmark_min_time=1m` on this setup; `60s` is the accepted one-minute form.
- Re-run the benchmark on an otherwise idle machine if you want cleaner absolute numbers.
