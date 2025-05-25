add_library(concurrent_queue_lib INTERFACE)

target_include_directories(concurrent_queue_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(concurrent_queue_lib INTERFACE Threads::Threads)

add_executable(concurrent_queue_test tests/test_concurrent_queue.cpp)
target_link_libraries(concurrent_queue_test PRIVATE
        concurrent_queue_lib
        GTest::gtest_main
        Threads::Threads
)

#add_executable(performance_test tests/performance_test.cpp)
#target_link_libraries(performance_test PRIVATE
#        concurrent_queue_lib
#        Threads::Threads
#)