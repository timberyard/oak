cmake_minimum_required(VERSION 2.6)
project(oak)

set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

include( ExternalProject )

set_directory_properties(properties EP_PREFIX "${PROJECT_BINARY_DIR}/external_projects")

include(zlib)
include(bzip2)
include(boost)
if(NOT WIN32)
	include(mongocpp)
endif()

set(Boost_LIBRARIES boost_system boost_filesystem boost_iostreams boost_program_options boost_thread boost_regex)

# Json Spirit
set( JsonSpiritArgs -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} -DBOOST_ROOT:STRING=${BOOST_ROOT} )
ExternalProject_Add(JsonSpirit
    GIT_REPOSITORY "git@github.com:everbase/json_spirit.git"
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

include_directories( ${BOOST_ROOT}/include )
include_directories( ${PROJECT_SOURCE_DIR}/../libs/boost-process)
include_directories( ${PROJECT_SOURCE_DIR}/../libs)
include_directories( ${JsonSpirit_INCLUDE_DIR} )
if(NOT WIN32)
	include_directories( ${MONGODB_CXX_DRIVER_INCLUDE} )
endif()

link_directories(${BOOST_ROOT}/lib)

add_definitions(-std=c++11)

if(APPLE)
	SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreServices" )
endif()

find_package (Threads)

add_custom_command(OUTPUT config_base.cpp
	COMMAND xxd -i configs/builtin/base.json ${PROJECT_BINARY_DIR}/config_base.cpp
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/base.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_custom_command(OUTPUT config_variant-c++.cpp
	COMMAND xxd -i configs/builtin/variant-c++.json ${PROJECT_BINARY_DIR}/config_variant-c++.cpp
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/variant-c++.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_custom_command(OUTPUT config_variant-greenfield.cpp
	COMMAND xxd -i configs/builtin/variant-greenfield.json ${PROJECT_BINARY_DIR}/config_variant-greenfield.cpp
	DEPENDS ${PROJECT_SOURCE_DIR}/../configs/builtin/variant-greenfield.json
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/../)

add_executable(oak
	main.cpp process.cpp tasks.cpp task_utils.cpp config.cpp
	config_base.cpp
	config_variant-c++.cpp config_variant-greenfield.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/model.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/operations.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/reader_bson.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/reader_json.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/uon.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/writer_bson.cpp
	${PROJECT_SOURCE_DIR}/../libs/uon/writer_json.cpp
)

add_dependencies(oak boost JsonSpirit)

if(NOT WIN32)
	add_dependencies(oak mongodb_cxx_driver)
endif()

if(NOT WIN32)
	target_link_libraries( oak ${MONGODB_CXX_DRIVER_LIBRARIES} ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} )
else()
	target_link_libraries( oak ${MONGODB_CXX_DRIVER_LIBRARIES} ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ws2_32 mswsock )
endif()

install(TARGETS oak DESTINATION bin )
