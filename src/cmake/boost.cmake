# Build boost via its bootstrap script. The build tree cannot contain a space.
# This boost b2 build system yields errors with spaces in the name of the
# build dir.

if( "${CMAKE_CURRENT_BINARY_DIR}" MATCHES " ")
    message( FATAL_ERROR "cannot use boost bootstrap with a space in the name of the build dir" )
endif()

set( TARGET_NAME "boost" )
set( _v "56" )
set( BOOST_URL "http://sourceforge.net/projects/boost/files/boost/1.${_v}.0/boost_1_${_v}_0.tar.gz/download" )
set( BOOST_MD5 "8c54705c424513fa2be0042696a3a162" )

# set build variant
if( CMAKE_BUILD_TYPE MATCHES "Debug" )
    set( BOOST_VARIANT "debug" )
else()
    set( BOOST_VARIANT "release" )
endif()

# define toolsets for b2 to use (this provides the paths defined in CMAKE_CXX_COMPILER)
configure_file(${CMAKE_SOURCE_DIR}/cmake/boost_user-config.jam.in ${CMAKE_CURRENT_BINARY_DIR}/external_projects/boost_user-config.jam)

# select one of the toolsets we just defined
if( CMAKE_CXX_COMPILER MATCHES "clang" )
    set( TOOLSET "clang-selected" )
else()
    set( TOOLSET "gcc-selected" )
endif()

# set windows toolset
if( WIN32 )
	if( MSVC )
		set( TOOLSET "msvc-selected" )
	endif()
endif()

set( ARM_OPTIONS "")

if( CMAKE_CXX_COMPILER MATCHES "arm" )
    set( ARM_OPTIONS
        architecture=arm
        abi=aapcs
        binary-format=elf
        target-os=linux
    )
endif()

# ./b2 arguments
set( B2_ARGS
	-q -j4
	warnings=off
	address-model=${TARGET_BITNESS}
	link=static
	threading=multi
	variant=${BOOST_VARIANT}
	${ARM_OPTIONS}
	--layout=system
	--without-python
	--without-mpi
	--without-coroutine
	--without-context
	-s BZIP2_INCLUDE=${CMAKE_CURRENT_BINARY_DIR}/external_projects/include
	-s BZIP2_LIBPATH=${CMAKE_CURRENT_BINARY_DIR}/external_projects/lib/
	-s ZLIB_INCLUDE=${CMAKE_CURRENT_BINARY_DIR}/external_projects/include
	-s ZLIB_LIBPATH=${CMAKE_CURRENT_BINARY_DIR}/external_projects/lib/
)

# set external project commands
# don't provide toolset to bootstrap, since it should just build with local achitecture
if( MSVC )
    set( BOOST_CMDS
	    CONFIGURE_COMMAND bootstrap.bat
	    BUILD_COMMAND b2 toolset=${TOOLSET} ${B2_ARGS}
	    INSTALL_COMMAND b2 ${B2_ARGS} --prefix=<INSTALL_DIR> install
    )
else()
	if( WIN32 )
		set( BOOST_CMDS
			CONFIGURE_COMMAND ./bootstrap.sh --with-toolset=mingw  > /dev/null COMMAND sed -i.bak "s/mingw/gcc/g" <SOURCE_DIR>/project-config.jam
			BUILD_COMMAND ./b2 toolset=${TOOLSET} ${B2_ARGS}  > /dev/null
			INSTALL_COMMAND ./b2 ${B2_ARGS} --prefix=<INSTALL_DIR> install > /dev/null
		)
	else()
		set( BOOST_CMDS
			CONFIGURE_COMMAND ./bootstrap.sh > /dev/null
			BUILD_COMMAND ./b2 toolset=${TOOLSET} ${B2_ARGS} > /dev/null
			INSTALL_COMMAND ./b2 ${B2_ARGS} --prefix=<INSTALL_DIR> install > /dev/null
		)
	endif()
endif()

ExternalProject_Add( ${TARGET_NAME}
    DEPENDS "bzip2" "zlib"
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL ${BOOST_URL}
    URL_MD5 ${BOOST_MD5}
    ${BOOST_CMDS}
    UPDATE_COMMAND ""
    BUILD_IN_SOURCE 1 # build in source so that ./bootstrap and ./b2 works
)

ExternalProject_Add_Step( ${TARGET_NAME} "prebuild"
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/external_projects/boost_user-config.jam <BINARY_DIR>/tools/build/src/user-config.jam
    COMMENT "Putting user-config.jam in place"
    DEPENDEES configure
    DEPENDERS build
    WORKING_DIRECTORY <SOURCE_DIR>
)

add_dependencies(${TARGET_NAME} zlib bzip2)

ExternalProject_Get_Property( ${TARGET_NAME} install_dir )
set( BOOST_ROOT "${install_dir}" CACHE INTERNAL "" )
