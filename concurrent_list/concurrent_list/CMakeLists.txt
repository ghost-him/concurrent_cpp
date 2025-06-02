add_library(concurrent_list_lib_v1 INTERFACE)

target_include_directories(concurrent_list_lib_v1 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(concurrent_list_lib_v1 INTERFACE Threads::Threads)

add_executable(concurrent_list_test tests/test_concurrent_list.cpp)
target_link_libraries(concurrent_list_test PRIVATE
        concurrent_list_lib_v1
        GTest::gtest_main
        Threads::Threads
)

#add_executable(performance_test tests/performance_test.cpp)
#target_link_libraries(performance_test PRIVATE
#        concurrent_list_lib_v1
#        Threads::Threads
#)