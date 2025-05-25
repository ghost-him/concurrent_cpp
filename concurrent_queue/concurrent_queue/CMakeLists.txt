add_library(concurrent_queue_lib
        concurrent_queue.cpp
        concurrent_queue.h
)

target_include_directories(concurrent_queue_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(concurrent_queue_lib PRIVATE Threads::Threads)

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