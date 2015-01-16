# An external project for zlib

set( TARGET_NAME "zlib" )
set( ZLIB_ROOT "${CMAKE_CURRENT_BINARY_DIR}/dependencies" )

set( ZLIB_VERSION "1.2.8")
set( ZLIB_URL "http://zlib.net/zlib-${ZLIB_VERSION}.tar.gz" )
set( ZLIB_MD5 "44d667c142d7cda120332623eab69f40" )

# If Windows we use CMake, otherwise ./configure
if(WIN32)
#    message( FATAL_ERROR "zlib build on WIN32 not implemented. See cmake/External_zlib.cmake." )
#    get_filename_component(_self_dir ${CMAKE_CURRENT_LIST_FILE} PATH)
#
#    ExternalProject_Add( "zlib"
#	DOWNLOAD_DIR ${DOWNLOAD_DIR}
#	PREFIX ${ZLIB_ROOT}
#	URL ${ZLIB_URL}
#	URL_MD5 ${ZLIB_MD5}
#        PATCH_COMMAND ${CMAKE_COMMAND} -E remove <SOURCE_DIR>/zconf.h
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                "${_self_dir}/zlib.CMakeLists.txt"
#                "<SOURCE_DIR>/CMakeLists.txt"
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                "${_self_dir}/zlib.gzguts.h"
#                "<SOURCE_DIR>/gzguts.h"
#        CMAKE_CACHE_ARGS
#            -DCMAKE_CXX_FLAGS:STRING=${pv_tpl_cxx_flags}
#            -DCMAKE_C_FLAGS:STRING=${pv_tpl_c_flags}
#            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_CFG_INTDIR}
#            ${pv_tpl_compiler_args}
#            ${zlib_EXTRA_ARGS}
#        CMAKE_ARGS
#            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
#
#    # the zlib library should be named zlib1.lib not zlib.lib
#    ExternalProject_Add_Step( "zlib" "RenameLib"
#        COMMAND ${CMAKE_COMMAND} -E copy "${ZLIB_ROOT}/lib/zlib.lib"
#            "${ZLIB_ROOT}/lib/zlib1.lib"
#        DEPENDEES install)

    ExternalProject_Add( ${TARGET_NAME}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		PREFIX ${ZLIB_ROOT}
		URL ${ZLIB_URL}
		URL_MD5 ${ZLIB_MD5}
	    BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND ""
		BUILD_COMMAND make -fwin32/Makefile.gcc CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=-fPIC DESTDIR=<INSTALL_DIR> INCLUDE_PATH=/include LIBRARY_PATH=/lib
		INSTALL_COMMAND make -fwin32/Makefile.gcc CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=-fPIC DESTDIR=<INSTALL_DIR> INCLUDE_PATH=/include LIBRARY_PATH=/lib install
    )
else()
    ExternalProject_Add( ${TARGET_NAME}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		PREFIX ${ZLIB_ROOT}
		URL ${ZLIB_URL}
		URL_MD5 ${ZLIB_MD5}
	    UILD_IN_SOURCE 1
		CONFIGURE_COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --static
		BUILD_COMMAND make CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=-fPIC
    )
endif()
