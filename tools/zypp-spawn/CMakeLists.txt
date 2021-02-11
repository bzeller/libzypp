PROJECT( zypp-spawn C CXX )

set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

include(FindPkgConfig)

pkg_check_modules ( SIGCPP REQUIRED sigc++-2.0 )
INCLUDE_DIRECTORIES( ${SIGCPP_INCLUDE_DIRS} )

pkg_check_modules ( LIBGLIB REQUIRED glib-2.0 )
INCLUDE_DIRECTORIES( ${LIBGLIB_INCLUDE_DIRS} )

FIND_PACKAGE(Protobuf REQUIRED)

SET( SOURCES main.cc server.cc clientconnection.cc )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} zypp-protobuf )
target_link_libraries( ${PROJECT_NAME} zypp-core )
target_link_libraries( ${PROJECT_NAME} ${LIBGLIB_LIBRARIES} )
target_link_libraries( ${PROJECT_NAME} ${SIGCPP_LIBRARIES} )
target_link_libraries( ${PROJECT_NAME} ${PROTOBUF_LITE_LIBRARIES} )
