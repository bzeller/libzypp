project(zyppng)

SET ( CMAKE_CXX_STANDARD 20 )
SET ( CMAKE_CXX_EXTENSIONS OFF )

include( ../zypp-logic/CMakeLists.txt )


SET ( zyppng_headers
  application.h
  context.h
)

SET ( zyppng_srcs
  application.cc
  context.cc
)


SET ( zyppng_lib_HEADERS
  ${zypp_logic_headers}
  ${zyppng_headers}
)

SET ( zyppng_lib_SRCS
  ${zypp_logic_srcs}
  ${zyppng_srcs}
)

# Default loggroup for all files
#SET_LOGGROUP( "zypp" ${zyppng_lib_SRCS} )

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  message(NOTICE "Enabling static analysis: ${ZYPP_CXX_CLANG_TIDY} ${ZYPP_CXX_CPPCHECK}")

  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

# due to limitations in cmake object libraries feature( object files are not derived transiently, they are only added to the target if they are added DIRECTLY)
# we have this function that adds all zypp objects to a specified target.
function(add_zyppng_lib target shared_or_static )

  set ( _link_mode PRIVATE )
  if ( ${shared_or_static} STREQUAL STATIC )
    message(NOTICE "Building ${target} static")
    set ( _link_mode INTERFACE )
  else()
    message(NOTICE "Building ${target} shared")
  endif()

  add_library( ${target} ${shared_or_static} )

  set_target_properties ( ${target} PROPERTIES VERSION "${LIBZYPP_VERSION_INFO}" )
  set_target_properties ( ${target} PROPERTIES SOVERSION "${LIBZYPP_SOVERSION_INFO}" )

  target_link_libraries( ${target} PRIVATE zypp_lib_compiler_flags )
  target_link_libraries( ${target} PRIVATE zyppng-objlib )
  target_link_libraries( ${target} PRIVATE commit-proto-obj )

  # CAUTION, link those libs always first and always in that order,
  # otherwise you get symbol lookup errors when linking against libzypp
  target_link_libraries( ${target} ${_link_mode} zypp-common )
  target_link_libraries( ${target} ${_link_mode} zypp-media )
  target_link_libraries( ${target} ${_link_mode} zypp-core )

  target_link_libraries( ${target} ${_link_mode} ${LibSolv_LIBRARIES} )
  target_link_libraries( ${target} ${_link_mode} ${RPM_LIBRARY} ${RPMIO_LIBRARY} )
  target_link_libraries( ${target} ${_link_mode} ${GETTEXT_LIBRARIES} )
  target_link_libraries( ${target} ${_link_mode} ${Boost_THREAD_LIBRARY})
  target_link_libraries( ${target} ${_link_mode} ${YAML_CPP_LIBRARIES})

  IF ( UDEV_FOUND )
    TARGET_LINK_LIBRARIES( ${target} ${_link_mode} ${UDEV_LIBRARY} )
  ELSE ( UDEV_FOUND )
    IF ( HAL_FOUND )
      TARGET_LINK_LIBRARIES( ${target} ${_link_mode} ${HAL_LIBRARY} ${HAL_STORAGE_LIBRARY} ${DBUS_LIBRARY} )
    ENDIF ( HAL_FOUND )
  ENDIF ( UDEV_FOUND )
endfunction()

ADD_LIBRARY( zyppng-objlib OBJECT ${zyppng_lib_SRCS} ${zyppng_lib_HEADERS}  )

target_link_libraries( zyppng-objlib PRIVATE zypp_lib_compiler_flags )

target_include_directories( zyppng-objlib PUBLIC ${RPM_INCLUDE_DIR})

# fix includes not relative to rpm
target_include_directories( zyppng-objlib PUBLIC ${RPM_INCLUDE_DIR}/rpm)

target_compile_definitions( zyppng-objlib PUBLIC ZYPP_USE_CORO LIBSOLV_SOLVABLE_PREPEND_DEP )

# rpm verify function and callback states were introduced in rpm-4.15
if( RPM_LIB_VER VERSION_GREATER_EQUAL "4.15.0"  AND  RPM_LIB_VER VERSION_LESS "5.0.0")
  target_compile_definitions( zyppng-objlib PUBLIC HAVE_RPM_VERIFY_TRANSACTION_STEP )
endif()

if( RPM_LIB_VER VERSION_GREATER_EQUAL "5.0.0" )
      MESSAGE( STATUS "rpm found: enable rpm-4 compat interface." )
      target_compile_definitions( zyppng-objlib PUBLIC _RPM_5)
endif ()

add_zyppng_lib( zyppng SHARED )
