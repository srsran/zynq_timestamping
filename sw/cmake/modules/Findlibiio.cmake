#
# Copyright 2013-2022 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

# - Try to find libiio
# Once done this will define
#
#  LIBIIO_FOUND - system has libiio
#  LIBIIO_INCLUDE_DIRS - the libiio include directory
#  LIBIIO_LIBRARIES - Link these to use libiio
#  LIBIIO_DEFINITIONS - Compiler switches required for using libiio
#

FIND_PACKAGE(PkgConfig REQUIRED)

pkg_check_modules(PC_LIBIIO QUIET libiio)
set(LIBIIO_DEFINITIONS ${PC_LIBIIO_CFLAGS_OTHER})

find_path(LIBIIO_INCLUDE_DIR iio.h
          HINTS ${PC_LIBIIO_INCLUDEDIR} ${PC_LIBIIO_INCLUDE_DIRS}
          PATH_SUFFIXES libiio)

find_library(LIBIIO_LIBRARY NAMES iio libiio
             HINTS ${PC_LIBIIO_LIBDIR} ${PC_LIBIIO_LIBRARY_DIRS})

set(LIBIIO_VERSION ${PC_LIBIIO_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libiio
                                  REQUIRED_VARS LIBIIO_LIBRARY LIBIIO_INCLUDE_DIR
                                  VERSION_VAR LIBIIO_VERSION)

mark_as_advanced(LIBIIO_INCLUDE_DIR LIBIIO_LIBRARY)

set(LIBIIO_LIBRARIES ${LIBIIO_LIBRARY})
set(LIBIIO_INCLUDE_DIRS ${LIBIIO_INCLUDE_DIR})