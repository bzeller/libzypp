IF( NOT DISABLE_MEDIABACKEND_TESTS)
  ADD_TESTS(
    NetworkRequestDispatcher
    EvDownloader
    Provider
  )
ENDIF()

target_link_libraries( Provider_test $<TARGET_OBJECTS:tvm-protocol-obj> )

