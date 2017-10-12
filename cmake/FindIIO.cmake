#
# Locate libiio package
#
# This script defines the following variables
#  LIBIIO_FOUND - System has libiio
#  LIBIIO_INCLUDE_DIRS - The libiio include directories
#  LIBIIO_LIBRARIES - The libraries needed to use libiio
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
pkg_check_modules(PC_IIO QUIET libiio)

find_path(IIO_INCLUDE_DIR
    iio.h
    HINTS
    ${PC_IIO_INCLUDEDIR}
    ${PC_IIO_INCLUDE_DIRS}
    )

find_library(IIO_LIBRARY
    NAMES iio libiio
    HINTS
    ${PC_IIO_LIBDIR}
    ${PC_IIO_LIBRARY_DIRS}
    )

set(IIO_LIBRARIES    ${IIO_LIBRARY})
set(IIO_INCLUDE_DIRS ${IIO_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(IIO DEFAULT_MSG
    IIO_LIBRARY IIO_INCLUDE_DIR)

mark_as_advanced(
    IIO_INCLUDE_DIR
    IIO_INCLUDE_DIRS
    IIO_LIBRARY
    IIO_LIBRARIES
    )
