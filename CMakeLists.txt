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
cmake_dependent_option(warp_FETCH_BOOST "Fetch Boost via FetchContent if not available" OFF
    "NOT DEFINED ENV{BOOST_ROOT}" OFF)

if (warp_FETCH_BOOST)
    set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
    FetchContent_Declare(boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.84.0
        UPDATE_DISCONNECTED ON
        FIND_PACKAGE_ARGS COMPONENTS beast asio json)
    FetchContent_MakeAvailable(boost)
endif ()

set(LIBPQXX_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(LIBPQXX_TEST OFF CACHE BOOL "" FORCE)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 8.0.0
    UPDATE_DISCONNECTED ON)
FetchContent_MakeAvailable(libpqxx)

if (APPLE AND TARGET pqxx)
    target_compile_options(pqxx PRIVATE -Wno-c++11-narrowing)
endif()

if (warp_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1
        UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(benchmark)
endif ()

if (warp_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(googletest)
endif ()

find_package(Boost 1.90 REQUIRED COMPONENTS json)
find_package(PostgreSQL REQUIRED)
find_package(Threads REQUIRED)

set(warp_codegen_sources
    src/codegen/data_class_emitter.cpp
    src/codegen/generator.cpp
    src/codegen/model.cpp
    src/codegen/resource_emitter.cpp
    src/codegen/resource_stub_emitter.cpp
    src/codegen/spec_model.cpp
    src/codegen/spec_parser.cpp
    src/codegen/stub_generator.cpp)

add_library(
    warp_http SHARED
        ${warp_codegen_sources}
        src/http/server.cpp
        src/http/server_impl.cpp
        src/http/session/http_session.cpp
        src/http/session/coroutine_http_session.cpp
        src/http/listener/listener.cpp
        src/http/listener/coroutine_listener.cpp
        src/http/router/registry.cpp
        src/db/postgres/connection_config.cpp
        src/db/postgres/connection_pool.cpp
        src/db/postgres/detail/connection_pool_impl.cpp
        src/db/postgres/result.cpp)

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
        libpqxx::pqxx)

add_executable(
    warp_codegen_cli
        src/codegen/codegen_cli.cpp
        ${warp_codegen_sources})

set_target_properties(warp_codegen_cli PROPERTIES OUTPUT_NAME warp_codegen)

target_include_directories(
    warp_codegen_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS warp_http warp_codegen_cli
    EXPORT warpTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

install(DIRECTORY include/ DESTINATION include)

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
