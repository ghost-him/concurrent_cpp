add_library(concurrent_unordered_map_lib INTERFACE)

target_include_directories(concurrent_unordered_map_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(concurrent_unordered_map_lib INTERFACE Threads::Threads)


#add_executable(performance_test tests/performance_test.cpp)
#target_link_libraries(performance_test PRIVATE
#        concurrent_unordered_map
#        Threads::Threads
#)
