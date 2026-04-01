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

add_warp_gtest_suite(warp_http_unit_tests
    unit/request_test.cpp
    unit/response_test.cpp
    unit/registry_test.cpp
    unit/connection_config_test.cpp
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
)
