set(WARP_TLS_FIXTURE_DIR "${PROJECT_BINARY_DIR}/generated/tls")

set(WARP_TLS_FIXTURE_OUTPUTS
    "${WARP_TLS_FIXTURE_DIR}/test_ca.pem"
    "${WARP_TLS_FIXTURE_DIR}/test_server_identity.pem"
    "${WARP_TLS_FIXTURE_DIR}/rotation_ca.pem"
    "${WARP_TLS_FIXTURE_DIR}/rotation_source_a.bundle.pem"
    "${WARP_TLS_FIXTURE_DIR}/rotation_source_b.bundle.pem")

add_custom_command(
    OUTPUT ${WARP_TLS_FIXTURE_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WARP_TLS_FIXTURE_DIR}"
    COMMAND bash "${PROJECT_SOURCE_DIR}/scripts/generate_test_tls_fixtures.sh" "${WARP_TLS_FIXTURE_DIR}"
    DEPENDS "${PROJECT_SOURCE_DIR}/scripts/generate_test_tls_fixtures.sh"
    COMMENT "Generating TLS fixtures for Warp tests and benchmarks"
    VERBATIM)

add_custom_target(warp_tls_fixtures DEPENDS ${WARP_TLS_FIXTURE_OUTPUTS})
