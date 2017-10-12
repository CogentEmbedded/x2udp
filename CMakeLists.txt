#
# Daemon for converting SocketCAN packets to UDP packets.
#
# Copyright (c) 2015-2017 Cogent Embedded Inc. ALL RIGHTS RESERVED.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


cmake_minimum_required(VERSION 3.1)

project("x2udp" C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# require C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)

# search for additional CMake modules
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    )

############# OPTIONS ########################
# set default values for parameters
if(NOT DEFINED DEFAULT_INTERFACE)
    set(DEFAULT_INTERFACE  "eth1" CACHE STRING "Default network interface for transmitting packets.")
endif()

set(VERSION_MAJOR 2)
set(VERSION_MINOR 0)
set(VERSION_RELEASE 2)
set(${PROJECT_NAME}_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_RELEASE})
add_definitions("-DVERSION=\"${${PROJECT_NAME}_VERSION}\"")

# required packages
find_package(IIO REQUIRED)
find_package(libdaemon REQUIRED)
find_package(libconfig REQUIRED)
find_library(M_LIB m)
find_package(KernelHeaders REQUIRED)

################ ...add sources ######################
file(GLOB IIO2UDP_SOURCES
    src/iio2udp.c
    etc/iio2udp
    etc/init.d/iio2udp
    )

file(GLOB CAN2UDP_SOURCES
    src/can2udp.c
    etc/can2udp
    etc/init.d/can2udp
    )

include_directories(
    "${IIO_INCLUDE_DIRS}"
    "${LIBDAEMON_INCLUDE_DIRS}"
    "${LIBCONFIG_INCLUDE_DIR}"
    "${KERNELHEADERS_DIR}"
    include
    )

file(GLOB_RECURSE INC_ALL "*.h")

############### Pre-configure files ################
configure_file(etc/iio2udp "${CMAKE_CURRENT_BINARY_DIR}/etc/iio2udp" @ONLY)
configure_file(etc/can2udp "${CMAKE_CURRENT_BINARY_DIR}/etc/can2udp" @ONLY)

############### Compilation ########################
add_executable(iio2udp ${IIO2UDP_SOURCES} ${INC_ALL})
target_link_libraries(iio2udp
    "${IIO_LIBRARIES}"
    "${LIBDAEMON_LIBRARIES}"
    "${LIBCONFIG_LIBRARIES}"
    "${M_LIB}"
    )

add_executable(can2udp ${CAN2UDP_SOURCES} ${INC_ALL})
target_link_libraries(can2udp
    "${LIBDAEMON_LIBRARIES}"
    "${LIBCONFIG_LIBRARIES}"
    )

############## Installation ########################
install(TARGETS can2udp iio2udp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

install(FILES include/can2udp.h include/iio2udp.h  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    cmake/${PROJECT_NAME}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    PATH_VARS
    CMAKE_INSTALL_INCLUDEDIR
    CMAKE_INSTALL_LIBDIR
    )

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${${PROJECT_NAME}_VERSION}
    COMPATIBILITY SameMajorVersion
    )

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/etc/iio2udp" DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/etc/can2udp" DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
install(PROGRAMS etc/init.d/iio2udp DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/init.d)
install(PROGRAMS etc/init.d/can2udp DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/init.d)
