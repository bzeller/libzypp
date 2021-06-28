ADD_TESTS(
  EventLoop
  IOBuffer
  AsyncMedia
)

target_link_libraries( AsyncMedia_test boost_system )

ADD_SUBDIRECTORY( media )
ADD_SUBDIRECTORY( io )
