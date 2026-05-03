add_executable(warp_http_event_loop_benchmark)
target_sources(warp_http_event_loop_benchmark PRIVATE
    http_event_loop_benchmark_main.cpp
    http_event_loop_benchmark_support.cpp
    http_event_loop_round_trip_benchmark.cpp
    http_event_loop_db_sync_benchmark.cpp
)
target_link_libraries(warp_http_event_loop_benchmark PRIVATE
    warp_http
    benchmark::benchmark
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_compile_definitions(warp_http_event_loop_benchmark PRIVATE
    WARP_BENCHMARK_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    WARP_BENCHMARK_TLS_FIXTURE_DIR="${WARP_TLS_FIXTURE_DIR}"
)
add_dependencies(warp_http_event_loop_benchmark warp_tls_fixtures)

if (TARGET warp_db_postgres)
    target_sources(warp_http_event_loop_benchmark PRIVATE
        http_event_loop_db_benchmark.cpp
    )
    target_link_libraries(warp_http_event_loop_benchmark PRIVATE warp_db_postgres)
    target_compile_definitions(warp_http_event_loop_benchmark PRIVATE WARP_BENCHMARK_HAS_DB=1)
    if(WARP_BENCHMARK_SYNC_DB)
        message(STATUS "WARP_BENCHMARK_SYNC_DB flag found!")
        target_compile_definitions(warp_http_event_loop_benchmark PRIVATE WARP_BENCHMARK_SYNC_DB=1)
    else()
        message(STATUS "WARP_BENCHMARK_SYNC_DB flag NOT found!")
    endif()
endif ()
