add_library(unity STATIC ${CMAKE_SOURCE_DIR}/vendor/unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_SOURCE_DIR}/vendor/unity)

add_executable(test_arena test_arena.c)
target_link_libraries(test_arena PRIVATE cgraph_lib unity)
target_include_directories(test_arena PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_arena COMMAND test_arena)

add_executable(test_hash test_hash.c)
target_link_libraries(test_hash PRIVATE cgraph_lib unity)
target_include_directories(test_hash PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_hash COMMAND test_hash)

add_executable(test_graph test_graph.c)
target_link_libraries(test_graph PRIVATE cgraph_lib unity)
target_include_directories(test_graph PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_graph COMMAND test_graph)

add_executable(test_serialize test_serialize.c)
target_link_libraries(test_serialize PRIVATE cgraph_lib unity)
target_include_directories(test_serialize PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_serialize COMMAND test_serialize)

set(FIXTURE_PATH "${CMAKE_SOURCE_DIR}/tests/fixtures")

add_executable(test_compdb test_compdb.c)
target_link_libraries(test_compdb PRIVATE cgraph_lib unity)
target_include_directories(test_compdb PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_compdb PRIVATE FIXTURE_PATH="${FIXTURE_PATH}")
add_test(NAME test_compdb COMMAND test_compdb)

add_executable(test_parser test_parser.c)
target_link_libraries(test_parser PRIVATE cgraph_lib unity)
target_include_directories(test_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_parser PRIVATE FIXTURE_PATH="${FIXTURE_PATH}")
add_test(NAME test_parser COMMAND test_parser)
