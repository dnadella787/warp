#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./scripts/rotate_tls_pems.sh <active-pem> <pem-a> <pem-b> [interval-seconds]

Examples:
  ./scripts/rotate_tls_pems.sh examples/tls/localhost.bundle.pem /tmp/cert-a.pem /tmp/cert-b.pem
  ./scripts/rotate_tls_pems.sh examples/tls/localhost.bundle.pem /tmp/cert-a.pem /tmp/cert-b.pem 2

Behavior:
  - Replaces <active-pem> atomically with alternating copies of <pem-a> and <pem-b>
  - Runs forever until interrupted
  - Sleeps 5 seconds between rotations by default
EOF
}

if [[ $# -lt 3 || $# -gt 4 ]]; then
  usage >&2
  exit 1
fi

active_pem="$1"
pem_a="$2"
pem_b="$3"
interval_seconds="${4:-5}"

for pem in "$pem_a" "$pem_b"; do
  if [[ ! -f "$pem" ]]; then
    echo "missing PEM file: $pem" >&2
    exit 1
  fi
done

active_dir="$(dirname "$active_pem")"
active_name="$(basename "$active_pem")"
mkdir -p "$active_dir"

install_pem() {
  local source_pem="$1"
  local tmp_pem="${active_dir}/.${active_name}.tmp.$$"
  cp "$source_pem" "$tmp_pem"
  mv "$tmp_pem" "$active_pem"
}

next_source="$pem_a"
if [[ -f "$active_pem" ]]; then
  if cmp -s "$active_pem" "$pem_a"; then
    next_source="$pem_b"
  elif cmp -s "$active_pem" "$pem_b"; then
    next_source="$pem_a"
  fi
fi

while true; do
  install_pem "$next_source"
  echo "rotated $(date '+%Y-%m-%d %H:%M:%S') -> $next_source"

  if [[ "$next_source" == "$pem_a" ]]; then
    next_source="$pem_b"
  else
    next_source="$pem_a"
  fi

  sleep "$interval_seconds"
done
