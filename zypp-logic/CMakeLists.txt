
include_directories( ${CMAKE_CURRENT_LIST_DIR} )

set ( zyppng_logic_headers
)

set ( zyppng_logic_srcs
)

# all headers that should be installed and are public API
set ( zypp_public_logic_headers
)

set( zypp_logic_headers
  ${CMAKE_CURRENT_LIST_DIR}/zypp/zypp-defines.h
  ${zypp_public_logic_headers}
  ${zyppng_logic_headers}
)

set( zypp_logic_srcs
  ${zyppng_logic_srcs}
)



# tell the IDE to group the source files under zypp-logic
source_group( zypp-logic
  ${zypp_logic_headers} ${zypp_public_logic_headers} ${zypp_logic_srcs}
)
