cmake_minimum_required(VERSION 4.0)
project(concurrent_queue)

set(CMAKE_CXX_STANDARD 23 REQUIRED)

add_subdirectory(concurrent_queue)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip # Or specific commit/tag
)

FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)


add_executable(concurrent_queue_exe main.cpp)
target_link_libraries(concurrent_queue_exe PRIVATE
    concurrent_queue_lib
)