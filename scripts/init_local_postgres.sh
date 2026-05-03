#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compose_file="${repo_root}/docker/postgres/compose.yaml"
sql_file="${repo_root}/docker/postgres/init.sql"

container_name="${WARP_DB_DOCKER_CONTAINER:-warp-db}"
raw_db_host="${WARP_DB_HOST:-127.0.0.1}"
db_port="${WARP_DB_PORT:-5432}"
db_user="${WARP_DB_USER:-localdbusr}"
db_password="${WARP_DB_PASSWORD:-localdbpwd}"
db_name="${WARP_DB_NAME:-warp-db}"

normalize_local_host() {
	case "$1" in
	localhost)
		printf '%s\n' "127.0.0.1"
		;;
	*)
		printf '%s\n' "$1"
		;;
	esac
}

db_host="$(normalize_local_host "${raw_db_host}")"

docker_compose() {
	docker compose -f "${compose_file}" "$@"
}

wait_for_postgres() {
	echo "Waiting for PostgreSQL to accept connections..."
	for _ in $(seq 1 60); do
		if docker exec "${container_name}" pg_isready -U "${db_user}" -d postgres >/dev/null 2>&1; then
			return 0
		fi
		sleep 1
	done

	echo "PostgreSQL did not become ready in time." >&2
	return 1
}

ensure_database() {
	echo "Ensuring database ${db_name} exists..."
	docker exec -i "${container_name}" env PGPASSWORD="${db_password}" \
		psql -v ON_ERROR_STOP=1 -U "${db_user}" -d postgres <<EOF
SELECT format('CREATE DATABASE %I', '${db_name}')
WHERE NOT EXISTS (SELECT 1 FROM pg_database WHERE datname = '${db_name}')
\gexec
EOF
}

apply_schema() {
	echo "Applying schema and seed data from ${sql_file##${repo_root}/}..."
	docker exec -i "${container_name}" env PGPASSWORD="${db_password}" \
		psql -v ON_ERROR_STOP=1 -U "${db_user}" -d "${db_name}" -f - < "${sql_file}"
}

print_env() {
	cat <<EOF
export WARP_DB_HOST=${db_host}
export WARP_DB_PORT=${db_port}
export WARP_DB_USER=${db_user}
export WARP_DB_PASSWORD=${db_password}
export WARP_DB_NAME=${db_name}
EOF
}

usage() {
	cat <<EOF
Usage: scripts/init_local_postgres.sh [command]

Commands:
  up     Start the Dockerized PostgreSQL instance, wait for readiness, and apply the schema/data seed.
  init   Apply the schema/data seed to an already-running container.
  env    Print the WARP_DB_* environment variables for this local setup.
  psql   Open a psql shell inside the container.
  down   Stop the local PostgreSQL container.
  reset  Stop the container and remove its data volume.

Environment overrides:
  WARP_DB_HOST
  WARP_DB_PORT
  WARP_DB_USER
  WARP_DB_PASSWORD
  WARP_DB_NAME
  WARP_DB_DOCKER_CONTAINER
EOF
}

command="${1:-up}"

case "${command}" in
up)
	docker_compose up -d
	wait_for_postgres
	ensure_database
	apply_schema
	echo
	echo "Local PostgreSQL is ready."
	print_env
	;;
init)
	wait_for_postgres
	ensure_database
	apply_schema
	;;
env)
	print_env
	;;
psql)
	wait_for_postgres
	exec docker exec -it "${container_name}" env PGPASSWORD="${db_password}" \
		psql -U "${db_user}" -d "${db_name}"
	;;
down)
	docker_compose down
	;;
reset)
	docker_compose down -v
	;;
*)
	usage >&2
	exit 1
	;;
esac
