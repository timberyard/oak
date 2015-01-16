cmake_minimum_required(VERSION 2.6)
project(oak)

set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

include( ExternalProject )

set_directory_properties(properties EP_PREFIX "${PROJECT_BINARY_DIR}/external_projects")

include(zlib)
include(bzip2)
include(boost)

set(Boost_LIBRARIES boost_system boost_filesystem boost_iostreams boost_program_options)

# Json Spirit
set( JsonSpiritArgs -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} -DBOOST_ROOT:STRING=${BOOST_ROOT} )
ExternalProject_Add(JsonSpirit
    GIT_REPOSITORY "git@chicago.everbase.net:niklas/json_spirit.git"
    GIT_TAG "0.0.1"
    CMAKE_ARGS ${JsonSpiritArgs}
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    )

add_dependencies(JsonSpirit boost)

ExternalProject_Get_Property( JsonSpirit SOURCE_DIR )
set( JsonSpirit_INCLUDE_DIR ${SOURCE_DIR} )

ExternalProject_Get_Property( JsonSpirit BINARY_DIR )
set( JsonSpirit_LIBRARY "${BINARY_DIR}/json_spirit/libjson_spirit.a" )
# end of Json Spirit

include_directories( ${BOOST_ROOT}/include ${BOOST_ROOT}/include/boost-1_56 )
include_directories( ${PROJECT_SOURCE_DIR}/../libs/boost-process)
include_directories( ${JsonSpirit_INCLUDE_DIR} )

link_directories(${BOOST_ROOT}/lib)

add_definitions(-std=c++11)

add_custom_command(OUTPUT config_defaults.cpp
	COMMAND xxd -i configs/builtin/defaults.json ${PROJECT_BINARY_DIR}/config_defaults.cpp
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/defaults.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_custom_command(OUTPUT config_tasks-c++.cpp
	COMMAND xxd -i configs/builtin/tasks-c++.json ${PROJECT_BINARY_DIR}/config_tasks-c++.cpp
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/tasks-c++.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_executable(oak main.cpp process.cpp tasks.cpp task.utils.cpp config_defaults.cpp config_tasks-c++.cpp)

add_dependencies(oak boost)

if(NOT WIN32)
	target_link_libraries( oak ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} )
else()
	target_link_libraries( oak ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} ws2_32 mswsock )
endif()
