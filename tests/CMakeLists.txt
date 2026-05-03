include(GoogleTest)

function(add_warp_runtime_gtest_suite target)
    add_executable(${target})
    target_sources(${target} PRIVATE ${ARGN})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests
    )
    target_compile_definitions(${target} PRIVATE WARP_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_compile_definitions(${target} PRIVATE WARP_TEST_TLS_FIXTURE_DIR="${WARP_TLS_FIXTURE_DIR}")
    # Runtime-only tests should not pull in codegen implementation or optional DB libs.
    target_link_libraries(${target} PRIVATE warp_http warp_codegen_core GTest::gtest_main GTest::gmock)
    add_dependencies(${target} warp_tls_fixtures)
    gtest_discover_tests(${target}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
endfunction()

function(add_warp_codegen_gtest_suite target)
    add_executable(${target})
    target_sources(${target} PRIVATE ${ARGN})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests
    )
    target_compile_definitions(${target} PRIVATE WARP_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_compile_definitions(${target} PRIVATE WARP_TEST_TLS_FIXTURE_DIR="${WARP_TLS_FIXTURE_DIR}")
    target_link_libraries(${target} PRIVATE warp_http warp_codegen_core GTest::gtest_main)
    add_dependencies(${target} warp_tls_fixtures)
    gtest_discover_tests(${target}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
endfunction()

function(add_warp_compile_only_suite target)
    add_library(${target} STATIC)
    target_sources(${target} PRIVATE ${ARGN})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests
    )
    target_link_libraries(${target} PRIVATE warp_http warp_codegen_core warp_codegen_runtime)
endfunction()

set(GENERATED_CODEGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/codegen")
set(GENERATED_QUERY_ROUTING_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/query_routing")
set(GENERATED_OVERLOADED_HANDLER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/overloaded_handler")
set(GENERATED_MULTI_HEADER_PRIMARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/multi_header_primary")
set(GENERATED_MULTI_HEADER_SECONDARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/multi_header_secondary")
set(GENERATED_SINGLETON_REQUIRED_QUERY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/singleton_required_query")
set(GENERATED_NESTED_VALIDATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/nested_validation")

warp_generate_stubs(
    TARGET generated_users_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/examples/codegen/users_api.yaml"
    OUTPUT_DIR "${GENERATED_CODEGEN_DIR}"
    MODEL_HEADER "generated_users_api_types.hpp"
    RESOURCE_HEADER "generated_users_api_resources.hpp"
)

warp_generate_stubs(
    TARGET generated_query_routing_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen/query_routing_api.yaml"
    OUTPUT_DIR "${GENERATED_QUERY_ROUTING_DIR}"
    MODEL_HEADER "generated_query_routing_api_types.hpp"
    RESOURCE_HEADER "generated_query_routing_api_resources.hpp"
)

warp_generate_stubs(
    TARGET generated_overloaded_handler_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen/overloaded_handler_api.yaml"
    OUTPUT_DIR "${GENERATED_OVERLOADED_HANDLER_DIR}"
    MODEL_HEADER "generated_overloaded_handler_api_types.hpp"
    RESOURCE_HEADER "generated_overloaded_handler_api_resources.hpp"
)

warp_generate_stubs(
    TARGET generated_multi_header_primary_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen/multi_generated_header_primary_api.yaml"
    OUTPUT_DIR "${GENERATED_MULTI_HEADER_PRIMARY_DIR}"
    MODEL_HEADER "generated_multi_header_primary_api_types.hpp"
    RESOURCE_HEADER "generated_multi_header_primary_api_resources.hpp"
)

warp_generate_stubs(
    TARGET generated_multi_header_secondary_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen/multi_generated_header_secondary_api.yaml"
    OUTPUT_DIR "${GENERATED_MULTI_HEADER_SECONDARY_DIR}"
    MODEL_HEADER "generated_multi_header_secondary_api_types.hpp"
    RESOURCE_HEADER "generated_multi_header_secondary_api_resources.hpp"
)

warp_generate_stubs(
    TARGET generated_singleton_required_query_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen/singleton_required_query_api.yaml"
    OUTPUT_DIR "${GENERATED_SINGLETON_REQUIRED_QUERY_DIR}"
    MODEL_HEADER "generated_singleton_required_query_api_types.hpp"
    RESOURCE_HEADER "generated_singleton_required_query_api_resources.hpp"
)

warp_generate_stubs(
    TARGET generated_nested_validation_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen/nested_validation_api.yaml"
    OUTPUT_DIR "${GENERATED_NESTED_VALIDATION_DIR}"
    MODEL_HEADER "generated_nested_validation_api_types.hpp"
    RESOURCE_HEADER "generated_nested_validation_api_resources.hpp"
)

set(warp_http_unit_test_sources
    unit/logger_test.cpp
    unit/request_test.cpp
    unit/response_test.cpp
    unit/registry_test.cpp
    unit/interceptor_chain_test.cpp
    unit/ssl_context_test.cpp
    unit/tls_transport_test.cpp
    unit/codegen/json_object_contract_test.cpp
    unit/codegen/model_test.cpp
    unit/codegen/spec_parser_test.cpp
    unit/codegen/data_class_emitter_test.cpp
    unit/codegen/generator_test.cpp
    unit/codegen/resource_emitter_test.cpp
)

add_warp_codegen_gtest_suite(warp_http_unit_tests
    ${warp_http_unit_test_sources}
)
target_link_libraries(warp_http_unit_tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)

add_warp_runtime_gtest_suite(warp_http_smoke_tests
    smoke/body_and_response_builder_smoke_test.cpp
    smoke/server_builder_smoke_test.cpp
)

set(warp_http_integration_test_sources
    support/integration/http_integration_harness.cpp
    integration/http_pipelining_integration_test.cpp
    integration/http_connection_and_error_integration_test.cpp
    integration/http_tls_integration_test.cpp
    integration/generated_api_integration_test.cpp
    integration/generated_query_routing_integration_test.cpp
    integration/generated_singleton_required_query_integration_test.cpp
    integration/generated_nested_validation_integration_test.cpp
)

add_warp_runtime_gtest_suite(warp_http_integration_tests
    ${warp_http_integration_test_sources}
)
add_dependencies(warp_http_integration_tests generated_users_api_headers)
add_dependencies(warp_http_integration_tests generated_query_routing_api_headers)
add_dependencies(warp_http_integration_tests generated_singleton_required_query_api_headers)
add_dependencies(warp_http_integration_tests generated_nested_validation_api_headers)
target_include_directories(warp_http_integration_tests PRIVATE
    "${GENERATED_CODEGEN_DIR}"
    "${GENERATED_QUERY_ROUTING_DIR}"
    "${GENERATED_SINGLETON_REQUIRED_QUERY_DIR}"
    "${GENERATED_NESTED_VALIDATION_DIR}"
)
target_link_libraries(warp_http_integration_tests PRIVATE warp_codegen_runtime)
target_link_libraries(warp_http_integration_tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)

add_warp_compile_only_suite(warp_http_route_validation_compile
    compile/query_route_validation_compile.cpp
)
add_dependencies(warp_http_route_validation_compile generated_query_routing_api_headers)
target_include_directories(warp_http_route_validation_compile PRIVATE
    "${GENERATED_QUERY_ROUTING_DIR}"
)

add_warp_compile_only_suite(warp_http_overloaded_handler_compile
    compile/overloaded_handler_codegen_compile.cpp
)
add_dependencies(warp_http_overloaded_handler_compile generated_overloaded_handler_api_headers)
target_include_directories(warp_http_overloaded_handler_compile PRIVATE
    "${GENERATED_OVERLOADED_HANDLER_DIR}"
)

add_warp_compile_only_suite(warp_http_multi_generated_header_compile
    compile/multi_generated_header_coexistence_compile.cpp
)
add_dependencies(warp_http_multi_generated_header_compile
    generated_multi_header_primary_api_headers
    generated_multi_header_secondary_api_headers
)
target_include_directories(warp_http_multi_generated_header_compile PRIVATE
    "${GENERATED_MULTI_HEADER_PRIMARY_DIR}"
    "${GENERATED_MULTI_HEADER_SECONDARY_DIR}"
)

if (TARGET warp_db_postgres)
    add_warp_runtime_gtest_suite(warp_http_db_unit_tests
        unit/connection_config_test.cpp
    )
    target_link_libraries(warp_http_db_unit_tests PRIVATE warp_db_postgres)

    add_warp_runtime_gtest_suite(warp_http_db_integration_tests
        support/integration/http_integration_harness.cpp
        support/integration/http_db_test_support.cpp
        integration/http_db_integration_test.cpp
    )
    target_link_libraries(warp_http_db_integration_tests PRIVATE warp_db_postgres OpenSSL::SSL OpenSSL::Crypto)
endif ()
