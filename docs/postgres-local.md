# Local PostgreSQL Setup

Warp's DB integration tests only require a reachable PostgreSQL instance with valid `WARP_DB_*` credentials. The example server and DB benchmarks also expect an `exchanges` table with an `NYSE` row.

This repo now includes a small Docker-based local setup, modeled after the approach in `../stock-market-service`.

## Files

- Compose file: [docker/postgres/compose.yaml](../docker/postgres/compose.yaml)
- Seed schema/data: [docker/postgres/init.sql](../docker/postgres/init.sql)
- Wrapper script: [scripts/init_local_postgres.sh](../scripts/init_local_postgres.sh)

## Quick Start

Start PostgreSQL, wait for readiness, and apply the schema/data seed:

```bash
./scripts/init_local_postgres.sh
```

That will:

- start a `postgres:16` container named `warp-db` by default
- create or reuse a named Docker volume
- apply the schema and sample exchange rows from `docker/postgres/init.sql`
- print the `WARP_DB_*` exports to use for tests, examples, and benchmarks

Export the variables into your shell:

```bash
eval "$(./scripts/init_local_postgres.sh env)"
```

## Common Commands

```bash
./scripts/init_local_postgres.sh up
./scripts/init_local_postgres.sh init
./scripts/init_local_postgres.sh psql
./scripts/init_local_postgres.sh down
./scripts/init_local_postgres.sh reset
```

Command behavior:

- `up` starts the container, waits for readiness, and reapplies the idempotent seed
- `init` reapplies the schema/data seed to an already-running container
- `psql` opens an interactive `psql` shell inside the container
- `down` stops the container
- `reset` stops the container and removes the named Docker volume

## Default Credentials

```bash
export WARP_DB_HOST=127.0.0.1
export WARP_DB_PORT=5432
export WARP_DB_USER=localdbusr
export WARP_DB_PASSWORD=localdbpwd
export WARP_DB_NAME=warp-db
```

You can override those before running the script. The compose file and script both respect:

- `WARP_DB_HOST`
- `WARP_DB_PORT`
- `WARP_DB_USER`
- `WARP_DB_PASSWORD`
- `WARP_DB_NAME`
- `WARP_DB_DOCKER_CONTAINER`

`WARP_DB_HOST` defaults to `127.0.0.1` and only affects the client-side exports. Docker still binds the container to the selected local port on localhost.

## What Gets Seeded

The SQL file creates:

- `exchanges`
- `exchange_values`
- `securities`
- `security_prices`

It also inserts `NASDAQ` and `NYSE` into `exchanges`, which is enough for:

- the example `/db/{id}` server route
- the DB benchmark route `/db/exchanges/nyse`
- general local experimentation against a familiar stock-market schema

## Using It With Tests

Once the exports are in place, the DB integration suite can run with:

```bash
./build-test/tests/warp_http_db_integration_tests
```

If the variables are unset, the DB integration tests still skip as before.
