cmake_minimum_required(VERSION 3.20)

project(warp
    VERSION 0.1.0
    DESCRIPTION "High-performance HTTP framework built on Boost.Beast/Asio"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CMakeDependentOption)
include(FetchContent)

option(warp_BUILD_EXAMPLES "Build warp HTTP framework examples" ON)
option(warp_BUILD_TESTS "Build warp test suite" ON)
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

find_package(Boost 1.80 REQUIRED COMPONENTS json)
find_package(Threads REQUIRED)

add_library(
    warp_http
        src/http/server.cpp
        src/http/detail/server_impl.cpp
        src/http/detail/session.cpp
        src/net/core/io_context_pool.cpp
        src/net/http/json_support.cpp
        src/net/router/router.cpp)

target_include_directories(
    warp_http
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> 
        $<INSTALL_INTERFACE:include>)

target_link_libraries(
    warp_http
    PUBLIC
        Boost::json
    PRIVATE
        Boost::headers
        Threads::Threads)

install(TARGETS warp_http
    EXPORT warpTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

install(DIRECTORY include/ DESTINATION include)

install(EXPORT warpTargets NAMESPACE warp:: DESTINATION lib/cmake/warp)

if (warp_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (warp_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
