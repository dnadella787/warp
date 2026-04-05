add_executable(warp_http_event_loop_benchmark)
target_sources(warp_http_event_loop_benchmark PRIVATE
    http_event_loop_benchmark_main.cpp
    http_event_loop_benchmark_support.cpp
    http_event_loop_round_trip_benchmark.cpp
)
target_link_libraries(warp_http_event_loop_benchmark PRIVATE warp_http benchmark::benchmark)

if (TARGET warp_db_postgres)
    target_sources(warp_http_event_loop_benchmark PRIVATE
        http_event_loop_db_benchmark.cpp
    )
    target_link_libraries(warp_http_event_loop_benchmark PRIVATE warp_db_postgres)
    target_compile_definitions(warp_http_event_loop_benchmark PRIVATE WARP_BENCHMARK_HAS_DB=1)
endif ()
