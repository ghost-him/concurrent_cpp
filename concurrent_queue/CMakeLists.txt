cmake_minimum_required(VERSION 4.0)
project(concurrent_queue)

set(CMAKE_CXX_STANDARD 23 REQUIRED)

add_subdirectory(concurrent_queue_v1)
add_subdirectory(concurrent_queue_v2)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip # Or specific commit/tag
)

FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)


add_executable(concurrent_queue_exe main.cpp)
# 如果想要链接v2版本的并发队列，需要更改下面的版本号
target_link_libraries(concurrent_queue_exe PRIVATE
    concurrent_queue_lib_v1
)