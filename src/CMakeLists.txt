add_library(cgraph_lib STATIC
    util/arena.c
    util/hash.c
    graph/graph.c
    graph/serialize.c
    graph/metrics.c
    parse/compdb.c
    parse/parser.c
    query/traverse.c
    query/impact.c
    query/quality.c
    export/json.c
)
target_include_directories(cgraph_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCLANG_INCLUDE_DIR}
)
target_link_libraries(cgraph_lib PUBLIC ${LIBCLANG_LIBRARY})

add_executable(cgraph main.c)
target_link_libraries(cgraph PRIVATE cgraph_lib)
