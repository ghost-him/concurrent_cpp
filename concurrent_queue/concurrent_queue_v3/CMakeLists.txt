add_library(concurrent_queue_lib_v3 INTERFACE)

target_include_directories(concurrent_queue_lib_v3 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(concurrent_queue_lib_v3 INTERFACE Threads::Threads)

add_executable(concurrent_queue_test_v3 tests/test_concurrent_queue.cpp)
target_link_libraries(concurrent_queue_test_v3 PRIVATE
        concurrent_queue_lib_v3
        GTest::gtest_main
        Threads::Threads
        atomic
)

#add_executable(performance_test tests/performance_test.cpp)
#target_link_libraries(performance_test PRIVATE
#        concurrent_queue_lib_v3
#        Threads::Threads
#)