cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE "Debug")

include( ExternalProject )

project(ci-run)
set_directory_properties(properties EP_PREFIX "${PROJECT_BINARY_DIR}/external_projects")

find_package(Boost COMPONENTS system filesystem iostreams program_options REQUIRED)


# Json Spirit
set( JsonSpiritArgs -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} )
ExternalProject_Add(JsonSpirit
    GIT_REPOSITORY "git@chicago.everbase.net:niklas/json_spirit.git"
    GIT_TAG "0.0.1"
    CMAKE_ARGS ${JsonSpiritArgs}
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    )

ExternalProject_Get_Property( JsonSpirit SOURCE_DIR )
set( JsonSpirit_INCLUDE_DIR ${SOURCE_DIR} )

ExternalProject_Get_Property( JsonSpirit BINARY_DIR )
set( JsonSpirit_LIBRARY "${BINARY_DIR}/json_spirit/libjson_spirit.a" )
# end of Json Spirit


include_directories( ${Boost_INCLUDE_DIRS} )
include_directories( ${PROJECT_SOURCE_DIR}/../libs/boost-process)
include_directories( ${JsonSpirit_INCLUDE_DIR} )

add_definitions(-std=c++11)

add_executable(ci-run main.cpp process.cpp tasks.cpp task.utils.cpp json.cpp)
target_link_libraries( ci-run ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY})
