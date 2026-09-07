cmake_minimum_required(VERSION 4.3)
project(uac_test)

set(CMAKE_CXX_STANDARD 26)

add_subdirectory(simple_wav)
add_executable(uac_test main.cpp
    uac.hpp
    logging.hpp)

target_compile_options(uac_test PRIVATE -freflection)
target_link_libraries(uac_test PRIVATE simple_wav)

wut_create_rpx(uac_test)