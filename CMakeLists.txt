cmake_minimum_required(VERSION 3.14)

project(pebble-le-test LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Bluetooth Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Bluetooth Concurrent)

find_package(PkgConfig REQUIRED) # Include functions provided by PkgConfig module.
pkg_check_modules(DBUS REQUIRED dbus-1) # This calls pkgconfig with appropriate arguments
include_directories(${DBUS_INCLUDE_DIRS})

add_executable(pebble-le-test
  main.cpp
  entryclass.h entryclass.cpp
  gattserver.h gattserver.cpp
  gattpacket.h gattpacket.cpp
  device/devicehandler.h device/devicehandler.cpp
  device/devicelocator.h device/devicelocator.cpp
  device/deviceservice.h device/deviceservice.cpp
  device/dbuswatcher.h device/dbuswatcher.cpp
)
target_link_libraries(pebble-le-test Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Bluetooth ${DBUS_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS pebble-le-test
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
