#!/usr/bin/env bash
set -euo pipefail

if ! command -v openssl >/dev/null 2>&1; then
  echo "openssl is required to generate TLS test fixtures" >&2
  exit 1
fi

output_dir="${1:?usage: generate_test_tls_fixtures.sh <output-dir>}"
mkdir -p "${output_dir}"

ca_key="${output_dir}/test-ca.key"
ca_cert="${output_dir}/test_ca.pem"
ca_serial="${output_dir}/test_ca.srl"
server_key="${output_dir}/test-server.key"
server_csr="${output_dir}/test-server.csr"
server_cert="${output_dir}/test-server.crt"
server_ext="${output_dir}/test-server.ext"
server_bundle="${output_dir}/test_server_identity.pem"
rotation_ca_cert="${output_dir}/rotation_ca.pem"

cleanup() {
  rm -f "${ca_key}" "${ca_serial}" "${server_key}" "${server_csr}" "${server_cert}" "${server_ext}"
  rm -f "${output_dir}"/rotation_source_*.key "${output_dir}"/rotation_source_*.csr
  rm -f "${output_dir}"/rotation_source_*.crt "${output_dir}"/rotation_source_*.ext
}
trap cleanup EXIT

cat >"${server_ext}" <<'EOF'
basicConstraints=CA:FALSE
subjectAltName=DNS:localhost,IP:127.0.0.1
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF

openssl genrsa -out "${ca_key}" 2048 >/dev/null 2>&1
openssl req -x509 -new -key "${ca_key}" -sha256 -days 3650 -out "${ca_cert}" -subj "/CN=warp-test-ca" >/dev/null 2>&1

openssl genrsa -out "${server_key}" 2048 >/dev/null 2>&1
openssl req -new -key "${server_key}" -out "${server_csr}" -subj "/CN=localhost" >/dev/null 2>&1
openssl x509 -req -in "${server_csr}" -CA "${ca_cert}" -CAkey "${ca_key}" -CAcreateserial \
  -out "${server_cert}" -days 825 -sha256 -extfile "${server_ext}" >/dev/null 2>&1
cat "${server_cert}" "${server_key}" >"${server_bundle}"

cp "${ca_cert}" "${rotation_ca_cert}"

generate_rotation_bundle() {
  local name="$1"
  local key_path="${output_dir}/${name}.key"
  local csr_path="${output_dir}/${name}.csr"
  local cert_path="${output_dir}/${name}.crt"
  local ext_path="${output_dir}/${name}.ext"
  local bundle_path="${output_dir}/${name}.bundle.pem"

  cat >"${ext_path}" <<'EOF'
basicConstraints=CA:FALSE
subjectAltName=DNS:localhost,IP:127.0.0.1
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF

  openssl genrsa -out "${key_path}" 2048 >/dev/null 2>&1
  openssl req -new -key "${key_path}" -out "${csr_path}" -subj "/CN=localhost" >/dev/null 2>&1
  openssl x509 -req -in "${csr_path}" -CA "${ca_cert}" -CAkey "${ca_key}" -CAcreateserial \
    -out "${cert_path}" -days 825 -sha256 -extfile "${ext_path}" >/dev/null 2>&1
  cat "${cert_path}" "${key_path}" >"${bundle_path}"
}

generate_rotation_bundle "rotation_source_a"
generate_rotation_bundle "rotation_source_b"
