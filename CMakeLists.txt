cmake_minimum_required(VERSION 3.20)

project(warp
    VERSION 0.1.0
    DESCRIPTION "High-performance HTTP framework built on Boost.Beast/Asio"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(FetchContent)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/WarpCodegen.cmake)

option(warp_BUILD_EXAMPLES "Build warp HTTP framework examples" ON)
option(warp_BUILD_TESTS "Build warp test suite" ON)
option(warp_BUILD_BENCHMARKS "Build warp benchmark suite" OFF)
option(warp_BUILD_DB "Build optional PostgreSQL module" ON)
option(WARP_BENCHMARK_SYNC_DB "Enable Synchronous DB benchmarks" OFF)
cmake_dependent_option(warp_FETCH_BOOST "Fetch Boost via FetchContent if not available" OFF
    "NOT DEFINED ENV{BOOST_ROOT}" OFF)

if (warp_FETCH_BOOST)
    set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
            boost
            GIT_REPOSITORY https://github.com/boostorg/boost.git
            # Update boost version here and the find_package below together
            GIT_TAG boost-1.90.0
            UPDATE_DISCONNECTED ON
            FIND_PACKAGE_ARGS COMPONENTS beast asio json)
    FetchContent_MakeAvailable(boost)
endif ()

if (warp_BUILD_DB)
    set(LIBPQXX_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(LIBPQXX_TEST OFF CACHE BOOL "" FORCE)
    set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            libpqxx
            GIT_REPOSITORY https://github.com/jtv/libpqxx.git
            GIT_TAG 8.0.1
            UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(libpqxx)
endif ()

if (warp_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.5
            UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(benchmark)
endif ()

if (warp_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(googletest)
endif ()

if (APPLE)
    # apple sdk does not support std::from_char yet so we just
    # use the next fastest thing, which is fast_float. std impl
    # based on fast_float in fact.
    FetchContent_Declare(
            fast_float
            GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
            GIT_TAG tags/v8.2.4
            GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(fast_float)
endif ()

find_package(spdlog CONFIG QUIET)
if (NOT spdlog_FOUND)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_WARNINGS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.15.3
            UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(spdlog)
endif ()

find_package(Boost 1.90 REQUIRED COMPONENTS json)
find_package(Threads REQUIRED)
if (warp_BUILD_DB)
    find_package(PostgreSQL REQUIRED)
endif ()

set(warp_codegen_sources
    src/codegen/data_class_emitter.cpp
    src/codegen/generator.cpp
    src/codegen/model.cpp
    src/codegen/resource_emitter.cpp
    src/codegen/resource_stub_emitter.cpp
    src/codegen/spec_model.cpp
    src/codegen/spec_parser.cpp
    src/codegen/stub_generator.cpp)

set(warp_http_sources
        src/server/server.cpp
        src/server/server_builder.cpp
        src/server/server_impl.hpp
        src/server/session/callback_http_session.cpp
        src/server/session/coroutine_http_session.cpp
        src/server/listener/callback_listener.cpp
        src/server/listener/coroutine_listener.cpp
        src/server/router/registry.cpp)

set(warp_db_postgres_sources
        src/db/postgres/connection_config.cpp
        src/db/postgres/connection_pool.cpp
        src/db/postgres/detail/connection_pool_impl.cpp
        src/db/postgres/result.cpp)

add_library(
    warp_codegen_core
        ${warp_codegen_sources})

target_include_directories(
    warp_codegen_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(
    warp_codegen_core
    PUBLIC
        Boost::json
        Boost::headers)

if (APPLE)
    target_link_libraries(
            warp_codegen_core
            PUBLIC
            fast_float)
endif()

add_library(
    warp_http SHARED
        ${warp_http_sources})

target_include_directories(
    warp_http
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(
    warp_http
    PUBLIC
        Boost::json
        Boost::headers
    PRIVATE
        Threads::Threads
        spdlog::spdlog_header_only)

add_library(warp_codegen_runtime INTERFACE)
target_include_directories(
    warp_codegen_runtime
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(
    warp_codegen_runtime
    INTERFACE
        warp_http
        Boost::json
        Boost::headers)

if (warp_BUILD_DB)
    add_library(
        warp_db_postgres SHARED
            ${warp_db_postgres_sources})

    target_include_directories(
        warp_db_postgres
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

    target_link_libraries(
        warp_db_postgres
        PUBLIC
            Boost::json
            Boost::headers
        PRIVATE
            Threads::Threads
            libpqxx::pqxx)
endif ()

add_executable(
    warp_codegen_cli
        src/codegen/codegen_cli.cpp)

set_target_properties(warp_codegen_cli PROPERTIES OUTPUT_NAME warp_codegen)

target_include_directories(
    warp_codegen_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(
    warp_codegen_cli
    PRIVATE
        warp_codegen_core)

if (APPLE)
    find_program(WARP_CODESIGN_EXECUTABLE codesign REQUIRED)
    add_custom_command(
        TARGET warp_codegen_cli
        POST_BUILD
        COMMAND "${WARP_CODESIGN_EXECUTABLE}" --force --sign - "$<TARGET_FILE:warp_codegen_cli>"
        VERBATIM
        COMMENT "Ad hoc signing warp_codegen for local execution")
endif ()

set(warp_install_targets
    warp_codegen_core
    warp_http
    warp_codegen_cli)
if (warp_BUILD_DB)
    list(APPEND warp_install_targets warp_db_postgres)
endif ()

install(TARGETS ${warp_install_targets}
    EXPORT warpTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

set(warp_public_headers
    include/warp/warp.hpp
    include/warp/codegen/detail/type_traits.hpp
    include/warp/codegen/generator.hpp
    include/warp/codegen/http_adapter.hpp
    include/warp/codegen/json_object_contract.hpp
    include/warp/db/postgres/connection_config.hpp
    include/warp/db/postgres/connection_pool.hpp
    include/warp/db/postgres/result.hpp
    include/warp/http/body_builder.hpp
    include/warp/http/event_loop_mode.hpp
    include/warp/http/http.hpp
    include/warp/http/request.hpp
    include/warp/http/response.hpp
    include/warp/http/response_builder.hpp
    include/warp/http/string_map.hpp
    include/warp/server/router/query_constraints.hpp
    include/warp/server/router/route_fixed_string.hpp
    include/warp/server/router/route_path.hpp
    include/warp/server/router/route_spec.hpp
    include/warp/server/server.hpp
    include/warp/server/server_builder.hpp)

foreach(warp_public_header IN LISTS warp_public_headers)
    get_filename_component(warp_public_header_dir "${warp_public_header}" DIRECTORY)
    string(REPLACE "include/" "" warp_public_install_dir "${warp_public_header_dir}")
    install(FILES "${warp_public_header}" DESTINATION "include/${warp_public_install_dir}")
endforeach()

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/warpConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/warpConfig.cmake
    INSTALL_DESTINATION lib/cmake/warp)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/warpConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/warpConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/warpConfigVersion.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WarpCodegen.cmake
    DESTINATION lib/cmake/warp)

install(EXPORT warpTargets
    FILE warpTargets.cmake
    NAMESPACE warp::
    DESTINATION lib/cmake/warp)

if (warp_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (warp_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (warp_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif ()
