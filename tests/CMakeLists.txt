add_executable(warp_http_smoke)
target_sources(warp_http_smoke PRIVATE smoke.cpp)
target_link_libraries(warp_http_smoke PRIVATE warp_http)

add_test(NAME warp_http_smoke COMMAND warp_http_smoke)
