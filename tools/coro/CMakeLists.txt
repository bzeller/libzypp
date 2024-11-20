PROJECT( zypp-coro-test C CXX )

set ( CMAKE_CXX_STANDARD 20 )
SET ( CMAKE_CXX_EXTENSIONS OFF )

SET( SOURCES
  main.cc
)

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} zypp_exe_compiler_flags )
target_link_libraries( ${PROJECT_NAME} zypp-media )
target_link_libraries( ${PROJECT_NAME} zypp-core )
