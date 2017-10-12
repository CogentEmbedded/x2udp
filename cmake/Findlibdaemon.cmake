#
# Locate libdaemon package
#
# This script defines the following variables
#  LIBDAEMON_FOUND - System has libdaemon
#  LIBDAEMON_INCLUDE_DIRS - The libdaemon include directories
#  LIBDAEMON_LIBRARIES - The libraries needed to use libdaemon
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

find_package(PkgConfig)
pkg_check_modules(PC_LIBDAEMON QUIET libdaemon)

find_path(LIBDAEMON_INCLUDE_DIR
    daemon.h dfork.h
    HINTS
    ${PC_LIBDAEMON_INCLUDEDIR}
    ${PC_LIBDAEMON_INCLUDE_DIRS}
    PATH_SUFFIXES
    libdaemon
    )

find_library(LIBDAEMON_LIBRARY
    NAMES daemon libdaemon
    HINTS
    ${PC_LIBDAEMON_LIBDIR}
    ${PC_LIBDAEMON_LIBRARY_DIRS}
    )

set(LIBDAEMON_LIBRARIES    ${LIBDAEMON_LIBRARY})
set(LIBDAEMON_INCLUDE_DIRS ${LIBDAEMON_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(libdaemon DEFAULT_MSG
    LIBDAEMON_LIBRARY LIBDAEMON_INCLUDE_DIR)

mark_as_advanced(
    LIBDAEMON_INCLUDE_DIR
    LIBDAEMON_INCLUDE_DIRS
    LIBDAEMON_LIBRARY
    LIBDAEMON_LIBRARIES
    )
