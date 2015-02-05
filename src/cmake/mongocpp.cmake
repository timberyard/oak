
ExternalProject_Add(scons_embedded
    URL                 "http://downloads.sourceforge.net/project/scons/scons-local/2.3.4/scons-local-2.3.4.tar.gz"
    URL_MD5             "3b3f9830d630cd337d74780d42920183"
    UPDATE_COMMAND      ""
    PATCH_COMMAND       ""
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       ""
    INSTALL_COMMAND     ""
)

ExternalProject_Get_Property( scons_embedded SOURCE_DIR )
set( SCONS_DIR ${SOURCE_DIR} )

ExternalProject_Add(mongodb_cxx_driver
    GIT_REPOSITORY "https://github.com/mongodb/mongo-cxx-driver.git"
    GIT_TAG "legacy"
    UPDATE_COMMAND      ""
    PATCH_COMMAND       ""
    CONFIGURE_COMMAND   ""
    BUILD_COMMAND       python ${SCONS_DIR}/scons.py --c++11=on --libpath=${BOOST_ROOT}/lib --cpppath=${BOOST_ROOT}/include
    BUILD_IN_SOURCE     1
    INSTALL_COMMAND     python ${SCONS_DIR}/scons.py --c++11=on --libpath=${BOOST_ROOT}/lib --cpppath=${BOOST_ROOT}/include --prefix=<INSTALL_DIR> install
    )

add_dependencies(mongodb_cxx_driver scons_embedded boost)

ExternalProject_Get_Property( mongodb_cxx_driver INSTALL_DIR )
set( MONGODB_CXX_DRIVER_LIBRARIES ${INSTALL_DIR}/lib/libmongoclient.a )
set( MONGODB_CXX_DRIVER_INCLUDE ${INSTALL_DIR}/include )
