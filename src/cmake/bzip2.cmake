# An external project for bzip2

set( TARGET_NAME "bzip2" )
set( BZIP2_ROOT "${CMAKE_CURRENT_BINARY_DIR}/dependencies" )

set( BZIP2_VERSION "1.0.6-no_test" )
set( BZIP2_URL "https://github.com/everbase/bzip2.git" )

# If Windows we use NMake, otherwise ./configure
if(WIN32)
#    ExternalProject_Add( ${TARGET_NAME}
#        DOWNLOAD_DIR ${DOWNLOAD_DIR}
#        PREFIX ${BZIP2_ROOT}
#        GIT_REPOSITORY ${BZIP2_URL}
#        GIT_TAG ${BZIP2_VERSION}
#        CONFIGURE_COMMAND ""
#        BUILD_COMMAND nmake -f makefile.msc
#        INSTALL_COMMAND nmake -f makefile.msc install PREFIX=<INSTALL_DIR>
#    )
    ExternalProject_Add( ${TARGET_NAME}
        DOWNLOAD_DIR ${DOWNLOAD_DIR}
        PREFIX ${BZIP2_ROOT}
        GIT_REPOSITORY ${BZIP2_URL}
        GIT_TAG ${BZIP2_VERSION}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS='-fPIC'
        INSTALL_COMMAND make install PREFIX=<INSTALL_DIR>
    )
else()
    ExternalProject_Add( ${TARGET_NAME}
        DOWNLOAD_DIR ${DOWNLOAD_DIR}
        PREFIX ${BZIP2_ROOT}
        GIT_REPOSITORY ${BZIP2_URL}
        GIT_TAG ${BZIP2_VERSION}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS='-fPIC'
        INSTALL_COMMAND make install PREFIX=<INSTALL_DIR>
    )
endif()
