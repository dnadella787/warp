#!/usr/bin/env bash
set -euo pipefail

if ! command -v openssl >/dev/null 2>&1; then
  echo "openssl is required to generate local TLS certificates" >&2
  exit 1
fi

output_dir="${1:-examples/tls}"
ca_key="${output_dir}/localhost-ca.key"
ca_cert="${output_dir}/localhost-ca.pem"
ca_serial="${output_dir}/localhost-ca.srl"
server_key="${output_dir}/localhost.key"
server_csr="${output_dir}/localhost.csr"
server_cert="${output_dir}/localhost.crt"
server_bundle="${output_dir}/localhost.bundle.pem"
server_ext="${output_dir}/localhost.ext"

mkdir -p "${output_dir}"

cleanup() {
  rm -f "${server_csr}" "${server_ext}" "${ca_serial}"
}
trap cleanup EXIT

cat >"${server_ext}" <<'EOF'
basicConstraints=CA:FALSE
subjectAltName=DNS:localhost,IP:127.0.0.1
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF

openssl genrsa -out "${ca_key}" 2048
openssl req -x509 -new -key "${ca_key}" -sha256 -days 3650 -out "${ca_cert}" -subj "/CN=warp-local-ca"

openssl genrsa -out "${server_key}" 2048
openssl req -new -key "${server_key}" -out "${server_csr}" -subj "/CN=localhost"
openssl x509 -req -in "${server_csr}" -CA "${ca_cert}" -CAkey "${ca_key}" -CAcreateserial \
  -out "${server_cert}" -days 825 -sha256 -extfile "${server_ext}"

cat "${server_cert}" "${server_key}" >"${server_bundle}"

cat <<EOF
Generated local TLS assets in ${output_dir}

Server PEM bundle for Warp:
  ${server_bundle}

Client trust anchor for curl / openssl:
  ${ca_cert}

Example server:
  WARP_EXAMPLE_TLS=1 WARP_EXAMPLE_TLS_PEM=${server_bundle} ./build/examples/warp_example_server

curl:
  curl --cacert ${ca_cert} 'https://localhost:8080/hello?name=Bob'

openssl:
  openssl s_client -connect localhost:8080 -servername localhost -CAfile ${ca_cert}

Note:
  Prefer --cacert ${ca_cert}. Some curl/OpenSSL builds will also accept ${server_cert} as an explicit trust anchor,
  but the CA file is the stable option to document and share.
EOF
