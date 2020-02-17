cmake_minimum_required(VERSION 3.5)

project(zypp-graph-qt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions( -DQT_QML_DEBUG )

find_package(Qt5 COMPONENTS Core Quick QuickControls2 Widgets QuickWidgets Xml REQUIRED)

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
  mainwindow.cpp
  #edittestsetupform.cpp
  testcase/testcasesetup.cpp
  testcase/channel.cpp
  testcase/source.cpp
  testcase/forceinstall.cpp
  testcase/localechange.cpp
)

qt5_wrap_ui(SOURCES mainwindow.ui edittestsetupform.ui)

add_executable( zypp-graph-qt ${SOURCES} ${RC_FILES} )

target_compile_definitions(zypp-graph-qt
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(zypp-graph-qt
  PRIVATE Qt5::Core Qt5::Quick Qt5::QuickControls2 Qt5::Widgets Qt5::Xml Qt5::QuickWidgets zypp)
