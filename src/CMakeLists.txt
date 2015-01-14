cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE "Debug")

project(oak)

include( ExternalProject )
set_directory_properties(properties EP_PREFIX "${PROJECT_BINARY_DIR}/external_projects")

set(Boost_USE_STATIC_LIBS ON)
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

add_custom_command(OUTPUT config_defaults.o
	COMMAND ld -r -b binary -o ${PROJECT_BINARY_DIR}/config_defaults.o configs/builtin/defaults.json
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/defaults.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_custom_command(OUTPUT config_tasks-c++.o
	COMMAND ld -r -b binary -o ${PROJECT_BINARY_DIR}/config_tasks-c++.o configs/builtin/tasks-c++.json
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/tasks-c++.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_executable(oak main.cpp process.cpp tasks.cpp task.utils.cpp config_defaults.o config_tasks-c++.o)

if(NOT WIN32)
	target_link_libraries( oak ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} )
else()
	target_link_libraries( oak ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} ws2_32 mswsock )
endif()
