cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE "debug")

project(dummy)
add_definitions(-std=c++11 -Wall -Wextra -Wconversion -pedantic)
add_executable(dummy dummy.cpp)
add_executable(googletest googletest.cpp)
target_link_libraries(googletest gtest gtest_main pthread)
