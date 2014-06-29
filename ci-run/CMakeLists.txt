cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE "debug")

project(ci-run)

include_directories(/home/niklas/Libs/boost-process)

find_package(Boost COMPONENTS system filesystem iostreams REQUIRED)

add_definitions(-std=c++11)

add_executable(ci-run main.cpp process.cpp tasks.cpp task.utils.cpp json.cpp)

target_link_libraries( ci-run ${Boost_LIBRARIES} )
