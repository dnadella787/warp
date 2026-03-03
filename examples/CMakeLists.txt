add_executable(warp_example_server)
target_sources(warp_example_server
    PRIVATE
        server.cpp)
target_link_libraries(warp_example_server
    PRIVATE
        warp_http)

add_executable(warp_example_request)
target_sources(warp_example_request
    PRIVATE
        request.cpp)
target_link_libraries(warp_example_request
    PRIVATE
        warp_http)
