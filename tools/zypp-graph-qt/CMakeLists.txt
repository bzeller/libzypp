cmake_minimum_required(VERSION 3.5)

project(zypp-graph-qt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions( -DQT_QML_DEBUG )

find_package(Qt5 COMPONENTS Core Quick QuickControls2 REQUIRED)

find_package(Boost REQUIRED graph)

set( RC_FILES
  qml.qrc
)

set( SOURCES
  main.cpp
  graphmodel.cpp
  graphitem.cpp
  layouts/radiallayout.cpp
  zypppoolmodel.cpp
)

add_executable( zypp-graph-qt ${SOURCES} ${RC_FILES} )

target_compile_definitions(zypp-graph-qt
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(zypp-graph-qt
  PRIVATE Qt5::Core Qt5::Quick Qt5::QuickControls2 zypp)
