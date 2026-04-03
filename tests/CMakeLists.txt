include(GoogleTest)

function(add_warp_gtest_suite target)
    add_executable(${target})
    target_sources(${target} PRIVATE ${ARGN})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests
    )
    target_link_libraries(${target} PRIVATE warp_http GTest::gtest_main)
    gtest_discover_tests(${target})
endfunction()

set(GENERATED_CODEGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/codegen")

warp_generate_stubs(
    TARGET generated_users_api_headers
    SPEC "${PROJECT_SOURCE_DIR}/examples/codegen/users_api.yaml"
    OUTPUT_DIR "${GENERATED_CODEGEN_DIR}"
    MODEL_HEADER "generated_users_api_types.hpp"
    RESOURCE_HEADER "generated_users_api_resources.hpp"
)

add_warp_gtest_suite(warp_http_unit_tests
    unit/request_test.cpp
    unit/response_test.cpp
    unit/registry_test.cpp
    unit/connection_config_test.cpp
    unit/codegen/model_test.cpp
    unit/codegen/spec_parser_test.cpp
    unit/codegen/data_class_emitter_test.cpp
    unit/codegen/generator_test.cpp
    unit/codegen/resource_emitter_test.cpp
)

add_warp_gtest_suite(warp_http_smoke_tests
    smoke/body_and_response_builder_smoke_test.cpp
    smoke/server_builder_smoke_test.cpp
)

add_warp_gtest_suite(warp_http_integration_tests
    support/integration/http_integration_harness.cpp
    integration/http_pipelining_integration_test.cpp
    integration/http_connection_and_error_integration_test.cpp
    integration/http_db_integration_test.cpp
    integration/generated_api_integration_test.cpp
)
add_dependencies(warp_http_integration_tests generated_users_api_headers)
target_include_directories(warp_http_integration_tests PRIVATE
    "${GENERATED_CODEGEN_DIR}"
)
