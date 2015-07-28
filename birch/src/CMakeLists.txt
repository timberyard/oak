cmake_minimum_required(VERSION 2.6)
project(birch)

set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../src/cmake" )

include( ExternalProject )

set_directory_properties(properties EP_PREFIX "${PROJECT_BINARY_DIR}/external_projects")

include(zlib)
include(bzip2)
include(boost)
include(mongocpp)

set(Boost_LIBRARIES libboost_system.a libboost_filesystem.a libboost_iostreams.a libboost_program_options.a libboost_thread.a libboost_regex.a)

# Json Spirit
#set( JsonSpiritArgs -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} -DBOOST_ROOT:STRING=${BOOST_ROOT} )
set( JsonSpiritArgs -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE} )
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

include_directories( ${PROJECT_SOURCE_DIR}/../../src )
include_directories( ${BOOST_ROOT}/include )
include_directories( ${PROJECT_SOURCE_DIR}/../../libs/boost-process)
include_directories( ${PROJECT_SOURCE_DIR}/../../libs)
include_directories( ${JsonSpirit_INCLUDE_DIR} )
include_directories( ${MONGODB_CXX_DRIVER_INCLUDE} )

link_directories( ${BOOST_ROOT}/lib )

add_definitions(-std=c++11)

if(APPLE)
	SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreServices" )
endif()

find_package (Threads)

add_executable(birch
	main.cpp notify.cpp consolidate.cpp formatter.cpp
	${PROJECT_SOURCE_DIR}/../../src/process.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/model.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/operations.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/reader_bson.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/reader_json.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/uon.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/writer_bson.cpp
	${PROJECT_SOURCE_DIR}/../../libs/uon/writer_json.cpp
)

add_dependencies(birch boost JsonSpirit mongodb_cxx_driver)

if(NOT WIN32)
	target_link_libraries( birch ${MONGODB_CXX_DRIVER_LIBRARIES} ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} )
else()
	target_link_libraries( birch ${MONGODB_CXX_DRIVER_LIBRARIES} ${Boost_LIBRARIES} ${JsonSpirit_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ws2_32 mswsock )
endif()

install(TARGETS birch DESTINATION bin )
