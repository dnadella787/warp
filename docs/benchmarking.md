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
  --benchmark_min_time=1.5s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true
```

The benchmark source is [http_event_loop_benchmark.cpp](/Users/dnadella/Projects/warp/benchmarks/http_event_loop_benchmark.cpp).

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

The `/ping` route is intentionally simple so the comparison stays focused on the event-loop implementation rather than application logic.

The `/db/exchanges/nyse` route includes the PostgreSQL connection pool and one SQL query so you can see how the two event-loop modes behave when the handler suspends on database I/O.

## Results

### Minimal `/ping` Round Trip

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

### PostgreSQL `NYSE` Round Trip

Measured with the same machine and build tree, plus:

- `WARP_DB_HOST=localhost`
- `WARP_DB_PORT=5432`
- `WARP_DB_USER=localdbusr`
- `WARP_DB_PASSWORD=localdbpwd`
- `WARP_DB_NAME=api-db`

Observed aggregates for `GET /db/exchanges/nyse`:

| Mode | Mean round-trip time | Median round-trip time | Mean throughput | Median throughput |
| --- | ---: | ---: | ---: | ---: |
| Callbacks | 2212 us | 2204 us | 452.231 req/s | 453.621 req/s |
| Coroutines | 2130 us | 2112 us | 469.802 req/s | 473.443 req/s |

In this run, the coroutine event loop was about 4% faster on the DB-backed benchmark.

The exact Google Benchmark output captured was:

```text
BM_CallbackEventLoop_DbRoundTrip/0/min_time:1.000/real_time_mean    2212 us   bytes_per_second=33.564Ki/s items_per_second=452.231/s
BM_CallbackEventLoop_DbRoundTrip/0/min_time:1.000/real_time_median  2204 us   bytes_per_second=33.6672Ki/s items_per_second=453.621/s
BM_CallbackEventLoop_DbRoundTrip/0/min_time:1.000/real_time_stddev  38.0 us   bytes_per_second=582.459/s items_per_second=7.66393/s

BM_CoroutineEventLoop_DbRoundTrip/1/min_time:1.000/real_time_mean   2130 us   bytes_per_second=34.8681Ki/s items_per_second=469.802/s
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:1.000/real_time_median 2112 us   bytes_per_second=35.1384Ki/s items_per_second=473.443/s
BM_CoroutineEventLoop_DbRoundTrip/1/min_time:1.000/real_time_stddev 62.5 us   bytes_per_second=1.01275Ki/s items_per_second=13.6454/s
```

## Interpretation

This benchmark measures the overhead of the HTTP event loop itself under a very small handler.

- The callback path is currently the faster default for raw round-trip latency.
- The coroutine path remains close, but adds a small amount of scheduler/state-machine overhead in this scenario.
- These numbers are directional, not universal. The DB-backed benchmark shows that once the handler spends most of its time waiting on PostgreSQL, event-loop overhead becomes a much smaller part of total request time.

## Notes

- The benchmark uses real sockets on localhost, so it needs an environment where binding local ports is allowed.
- The DB-backed benchmark also requires a reachable PostgreSQL instance with an `exchanges` table and a row for `exchange_code = 'NYSE'` if you want a successful `200 OK` response.
- Google Benchmark on this macOS setup could not determine CPU frequency metadata correctly. That warning does not affect the measured wall-clock results above.
- Re-run the benchmark on an otherwise idle machine if you want cleaner absolute numbers.
