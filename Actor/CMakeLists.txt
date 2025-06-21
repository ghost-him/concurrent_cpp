cmake_minimum_required(VERSION 3.31)
project(actor)

set(CMAKE_CXX_STANDARD 23 REQUIRED)

add_subdirectory(Actor)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip # Or specific commit/tag
)

FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)


add_executable(actor_exe main.cpp)
target_link_libraries(actor_exe PRIVATE
        actor_lib_v1
)