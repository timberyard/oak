cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE "Debug")

project(ci-run)

find_package(Boost COMPONENTS system filesystem iostreams program_options REQUIRED)

include_directories( ${Boost_INCLUDE_DIRS} )
include_directories( ${PROJECT_SOURCE_DIR}/../libs/boost-process)

add_definitions(-std=c++11)

add_executable(ci-run main.cpp process.cpp tasks.cpp task.utils.cpp json.cpp)
target_link_libraries( ci-run ${Boost_LIBRARIES} )
