
include_directories( ${CMAKE_CURRENT_LIST_DIR})

SET( zypp_logic_headers ${CMAKE_CURRENT_LIST_DIR}/defines.h )
SET( zypp_logic_srcs     )

# tell the IDE to group the source files under zypp-logic
source_group( zypp-logic ${zypp_logic_headers} ${zypp_logic_srcs} )
