#
# Locate libconfig package
#
# This script defines the following variables
#  LIBCONFIG_FOUND - System has libconfig
#  LIBCONFIG_INCLUDE_DIRS - The libconfig include directories
#  LIBCONFIG_LIBRARIES - The libraries needed to use libconfig
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
pkg_check_modules(PC_LIBCONFIG QUIET libconfig)

find_path(LIBCONFIG_INCLUDE_DIR
    libconfig.h
    HINTS
    ${PC_LIBCONFIG_INCLUDEDIR}
    ${PC_LIBCONFIG_INCLUDE_DIRS}
    )

find_library(LIBCONFIG_LIBRARY
    NAMES config libconfig
    HINTS
    ${PC_LIBCONFIG_LIBDIR}
    ${PC_LIBCONFIG_LIBRARY_DIRS}
    )

set(LIBCONFIG_LIBRARIES    ${LIBCONFIG_LIBRARY})
set(LIBCONFIG_INCLUDE_DIRS ${LIBCONFIG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(libconfig DEFAULT_MSG
    LIBCONFIG_LIBRARY LIBCONFIG_INCLUDE_DIR)

mark_as_advanced(
    LIBCONFIG_INCLUDE_DIR
    LIBCONFIG_INCLUDE_DIRS
    LIBCONFIG_LIBRARY
    LIBCONFIG_LIBRARIES
    )
