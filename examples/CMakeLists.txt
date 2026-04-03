if (TARGET warp_db_postgres)
    add_executable(warp_example_server)
    target_sources(warp_example_server
        PRIVATE
            server.cpp)
    target_link_libraries(warp_example_server
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

add_executable(warp_example_generate_users_headers)
target_sources(warp_example_generate_users_headers
    PRIVATE
        codegen/generate_users_headers.cpp)
target_link_libraries(warp_example_generate_users_headers
    PRIVATE
        warp_codegen_core)
