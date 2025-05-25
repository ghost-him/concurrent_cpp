cmake_minimum_required(VERSION 4.0)
project(thread_pool)

set(CMAKE_CXX_STANDARD 23 REQUIRED)

add_subdirectory(thread_pool)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip # Or specific commit/tag
)

FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)


add_executable(thread_pool_exe main.cpp)
target_link_libraries(thread_pool_exe PRIVATE
        thread_pool_lib
)