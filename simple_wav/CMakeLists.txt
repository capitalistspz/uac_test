cmake_minimum_required(VERSION 4.3)
project(simple_wav C)

set(CMAKE_C_STANDARD 23)

add_library(simple_wav STATIC simple_wav.c)
target_include_directories(simple_wav PUBLIC .)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(simple_wav PRIVATE
        -Wall
        -Wextra
        -Wno-old-style-declaration) # Useless warnings about `auto`
endif ()