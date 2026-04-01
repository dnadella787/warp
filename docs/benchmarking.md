# Benchmarking

Warp includes a Google Benchmark target for comparing the callback-based event loop with the coroutine-based event loop.

The benchmark binary now contains two scenarios:

- a minimal in-memory `GET /ping` round trip
- a PostgreSQL-backed `GET /db/exchanges/nyse` round trip that queries `exchanges` for `exchange_code = 'NYSE'`

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
  --benchmark_min_time=60s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true \
  --benchmark_counters_tabular=true
```

The benchmark sources live in [benchmarks/](/Users/dnadella/Projects/warp/benchmarks), with shared support code in
[http_event_loop_benchmark_support.cpp](/Users/dnadella/Projects/warp/benchmarks/http_event_loop_benchmark_support.cpp)
and separate scenario files for the in-memory and DB-backed cases.

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

- `arm64`
- `Release` build
- `--benchmark_min_time=60s`
- 5 repetitions
- aggregate-only Google Benchmark reporting

Observed aggregates:

| Mode | Mean round-trip time | Median round-trip time | Mean throughput | Median throughput | Mean proc CPU / req | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Callbacks | 79.8 us | 79.6 us | 12.54k req/s | 12.56k req/s | 82.0 us | 7.57 MiB |
| Coroutines | 80.2 us | 80.1 us | 12.46k req/s | 12.49k req/s | 80.7 us | 7.66 MiB |

In this run, the callback event loop remained slightly faster on raw `/ping` round-trip latency, but the gap was under 1%.

The exact Google Benchmark aggregates captured were:

```text
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_mean    79.8 us   bytes_per_second=771.484Ki/s items_per_second=12.5397k/s proc_cpu_pct=102.761 proc_cpu_us_per_req=81.9719 rss_peak_mib=7.56563
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_median  79.6 us   bytes_per_second=772.488Ki/s items_per_second=12.556k/s  proc_cpu_pct=102.823 proc_cpu_us_per_req=82.9718 rss_peak_mib=7.39062
BM_CallbackEventLoop_RoundTrip/0/min_time:60.000/real_time_stddev  1.73 us   bytes_per_second=16.6878Ki/s items_per_second=271.243/s proc_cpu_pct=2.21112 proc_cpu_us_per_req=2.14672 rss_peak_mib=0.422077

BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_mean   80.2 us   bytes_per_second=766.884Ki/s items_per_second=12.4649k/s proc_cpu_pct=100.518 proc_cpu_us_per_req=80.6526 rss_peak_mib=7.6625
BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_median 80.1 us   bytes_per_second=768.326Ki/s items_per_second=12.4883k/s proc_cpu_pct=100.169 proc_cpu_us_per_req=79.7039 rss_peak_mib=7.65625
BM_CoroutineEventLoop_RoundTrip/1/min_time:60.000/real_time_stddev 0.812 us  bytes_per_second=7.69416Ki/s items_per_second=125.061/s proc_cpu_pct=1.02245 proc_cpu_us_per_req=1.57709 rss_peak_mib=8.55816m
```

### PostgreSQL `NYSE` Round Trip

Measured with the same machine and build tree, plus:

- `WARP_DB_HOST=localhost`
- `WARP_DB_PORT=5432`
- `WARP_DB_USER=localdbusr`
- `WARP_DB_PASSWORD=localdbpwd`
- `WARP_DB_NAME=api-db`

Observed aggregates for `GET /db/exchanges/nyse`:

| Mode | Mean round-trip time | Median round-trip time | Mean throughput | Median throughput | Mean proc CPU / req | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Callbacks | 2164 us | 2167 us | 462.109 req/s | 461.362 req/s | 193.5 us | 18.75 MiB |
| Coroutines | 2105 us | 2094 us | 475.321 req/s | 477.583 req/s | 196.4 us | 18.72 MiB |

In this run, the coroutine event loop was about 3% faster on the DB-backed benchmark.

The exact Google Benchmark output captured was:

```text
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_mean    2164 us   bytes_per_second=34.2971Ki/s items_per_second=462.109/s proc_cpu_pct=8.94046 proc_cpu_us_per_req=193.49 rss_peak_mib=18.7469
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_median  2167 us   bytes_per_second=34.2417Ki/s items_per_second=461.362/s proc_cpu_pct=8.92785 proc_cpu_us_per_req=192.977 rss_peak_mib=18.75
BM_CallbackEventLoop_DbRoundTrip/0/min_time:60.000/real_time_stddev  21.1 us   bytes_per_second=343.829/s items_per_second=4.52406/s proc_cpu_pct=0.0622825 proc_cpu_us_per_req=2.70193 rss_peak_mib=0.0356305

BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_mean   2105 us   bytes_per_second=35.2777Ki/s items_per_second=475.321/s proc_cpu_pct=9.33099 proc_cpu_us_per_req=196.391 rss_peak_mib=18.7188
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_median 2094 us   bytes_per_second=35.4456Ki/s items_per_second=477.583/s proc_cpu_pct=9.29202 proc_cpu_us_per_req=195.461 rss_peak_mib=18.7188
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:60.000/real_time_stddev 44.6 us   bytes_per_second=760.558/s items_per_second=10.0073/s proc_cpu_pct=0.139151 proc_cpu_us_per_req=5.65371 rss_peak_mib=0
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
