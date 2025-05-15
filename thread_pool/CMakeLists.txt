add_library(thread_pool_lib
        thread_pool.cpp
        thread_pool.h
        enable_singleton.cpp
        enable_singleton.h
)

target_include_directories(thread_pool_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(thread_pool_lib PRIVATE Threads::Threads)

add_executable(thread_pool_test tests/test_thread_pool.cpp)
target_link_libraries(thread_pool_test PRIVATE
        thread_pool_lib
        GTest::gtest_main
        Threads::Threads
)