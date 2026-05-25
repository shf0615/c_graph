cmake_minimum_required(VERSION 3.16)
project(cgraph C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CGRAPH_BUILD_TESTS "Build tests" ON)

# Find libclang
find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
    HINTS /usr/lib/llvm-18/include /usr/lib/llvm/include /usr/include)
find_library(LIBCLANG_LIBRARY NAMES clang clang-18
    HINTS /usr/lib/llvm-18/lib /usr/lib/x86_64-linux-gnu /usr/lib/llvm/lib)

if(NOT LIBCLANG_INCLUDE_DIR OR NOT LIBCLANG_LIBRARY)
    message(FATAL_ERROR "libclang not found. Install libclang-dev.")
endif()
message(STATUS "libclang include: ${LIBCLANG_INCLUDE_DIR}")
message(STATUS "libclang library: ${LIBCLANG_LIBRARY}")

add_subdirectory(src)

if(CGRAPH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
