add_library(cgraph_lib STATIC
    util/arena.c
    util/hash.c
    graph/graph.c
    graph/serialize.c
)
target_include_directories(cgraph_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cgraph main.c)
target_link_libraries(cgraph PRIVATE cgraph_lib)
