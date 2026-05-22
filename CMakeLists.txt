cmake_minimum_required(VERSION 3.16)
project(cgraph C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CGRAPH_BUILD_TESTS "Build tests" ON)

add_subdirectory(src)

if(CGRAPH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
