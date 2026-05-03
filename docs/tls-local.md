# Local TLS

Use [scripts/generate_local_tls_certs.sh](../scripts/generate_local_tls_certs.sh) to create a local CA, a `localhost` server certificate, and the single PEM bundle that Warp's current `file_cert_loader` expects.

## Generate certs

```bash
./scripts/generate_local_tls_certs.sh
```

This writes:

- `examples/tls/localhost.bundle.pem`: server certificate + private key for Warp
- `examples/tls/localhost-ca.pem`: CA certificate for client trust
- `examples/tls/localhost.crt`: server leaf certificate

## Run the example server with TLS

```bash
WARP_EXAMPLE_TLS=1 \
WARP_EXAMPLE_TLS_PEM=examples/tls/localhost.bundle.pem \
./build/examples/warp_example_server
```

Expected startup logs:

```text
Warp example server running on https://127.0.0.1:8080
TLS enabled with PEM bundle at examples/tls/localhost.bundle.pem
```

## Test locally

Trusted request:

```bash
curl --cacert examples/tls/localhost-ca.pem 'https://localhost:8080/hello?name=Bob'
```

Quick-and-dirty trust bypass:

```bash
curl -k 'https://localhost:8080/hello?name=Bob'
```

OpenSSL handshake inspection:

```bash
openssl s_client -connect localhost:8080 -servername localhost -CAfile examples/tls/localhost-ca.pem
```

## Expected errors

If TLS is disabled and you send `https://` traffic to the plain HTTP listener, the client handshake fails. On the server side, Beast may log `bad method` because the HTTP parser is trying to interpret TLS handshake bytes as an HTTP request line. That is expected for a protocol mismatch.

If TLS is enabled and the client does not trust the local CA, the client usually reports a self-signed or unknown-issuer failure and the server logs a TLS handshake error such as `tlsv1 alert unknown ca`. That is also expected: the client aborts the handshake and sends the alert back to the server.

Prefer `curl --cacert examples/tls/localhost-ca.pem`. Some curl/OpenSSL builds will also accept `examples/tls/localhost.crt` as an explicit trust anchor, but the CA file is the stable trust path to document and reuse.
