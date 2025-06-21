add_library(actor_lib_v1 INTERFACE
        concurrent_queue.h
        class_a.h
        class_b.h)

target_include_directories(actor_lib_v1 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(actor_lib_v1 INTERFACE Threads::Threads)