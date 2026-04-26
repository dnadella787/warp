if (TARGET warp_db_postgres)
    add_executable(warp_example_server)
    target_sources(warp_example_server
        PRIVATE
            server.cpp)
    target_link_libraries(warp_example_server
        PRIVATE
            warp_http
            warp_db_postgres)

    add_executable(warp_compile_time_server)
    target_sources(warp_compile_time_server
            PRIVATE
            compile_time.cpp)
    target_link_libraries(warp_compile_time_server
            PRIVATE
            warp_http
            warp_db_postgres)
endif ()

add_executable(warp_example_request)
target_sources(warp_example_request
    PRIVATE
        request.cpp)
target_link_libraries(warp_example_request
    PRIVATE
        warp_http
        Boost::headers)

add_executable(users_resource_example)
target_sources(users_resource_example
    PRIVATE
        codegen/users_resource_example.cpp)
target_link_libraries(users_resource_example
    PRIVATE
        warp_http
        warp_codegen_core)
