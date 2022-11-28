#
# Copyright 2013-2022 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#


########################################################################
# Prevent in-tree builds
########################################################################
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree build. This is bad practice.")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 3.10)
project( Zynq-timestamping )
message( STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM} )
message( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )
message( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
include(SRSRANVersion) #sets version information

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFORCE_STANDARD_RATE")

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
   message(STATUS "Build type not specified: defaulting to Release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

########################################################################
# Options
########################################################################
option(ENABLE_IIO     "Enable IIO"    OFF)
option(ENABLE_RFDC    "Enable RFdc"   OFF)

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  set(GCC_ARCH armv8-a CACHE STRING "GCC compile for specific architecture.")
  message(STATUS "Detected aarch64 processor")
else(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  set(GCC_ARCH native CACHE STRING "GCC compile for specific architecture.")
endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")

# Threads
find_package(Threads REQUIRED)

# libIIO
if (ENABLE_IIO)
  find_package(libiio 0.9)
  if (LIBIIO_FOUND)
    set(IIO_FOUND TRUE)
    set(RF_FOUND TRUE CACHE INTERNAL "RF frontend found")
    message(STATUS "LIBIIO_INCLUDE_DIRS: ${LIBIIO_INCLUDE_DIRS}")
    message(STATUS "LIBIIO_LIBRARIES: ${LIBIIO_LIBRARIES}")
    include_directories(${LIBIIO_INCLUDE_DIRS})

    # Find libad9361
    find_package(libad9361 REQUIRED)
    if (LIBAD9361_FOUND)
      message(STATUS "LIBAD9361_INCLUDE_DIRS: ${LIBAD9361_INCLUDE_DIRS}")
      message(STATUS "LIBAD9361_LIBRARIES: ${LIBAD9361_LIBRARIES}")
      message(STATUS "LIBAD9361_DEFINITIONS: ${LIBAD9361_DEFINITIONS}")
      include_directories(${LIBAD9361_INCLUDE_DIRS})
      add_definitions(-DHAS_AD9361_IIO)
    endif(LIBAD9361_FOUND)
  else(LIBIIO_FOUND)
    message(FATAL_ERROR "libiio not found")
  endif(LIBIIO_FOUND)
endif(ENABLE_IIO)

# Xilinx RFdc
if(ENABLE_RFDC)
  find_path(LIBMETAL_INCLUDE_DIR NAMES metal/sys.h)
  find_library(LIBMETAL_LIB NAMES metal)
  if(LIBMETAL_LIB)
    message(STATUS "libmetal library found")
    message(STATUS "  LIBMETAL_INCLUDE_DIR: ${LIBMETAL_INCLUDE_DIR}")
    message(STATUS "  LIBMETAL_LIBRARIES: ${LIBMETAL_LIB}")
    include_directories(${LIBMETAL_INCLUDE_DIR})

    # find RFDC library
    find_path(RFDC_INCLUDE_DIR NAMES xrfdc.h)
    find_library(RFDC_LIBRARY NAMES rfdc)
    if(RFDC_LIBRARY)
      message(STATUS "RFDC library found")
      message(STATUS "  RFDC_INCLUDE_DIR: ${RFDC_INCLUDE_DIR}")
      message(STATUS "  RFDC_LIBRARY: ${RFDC_LIBRARY}")
      include_directories(${RFDC_INCLUDE_DIR})
      set(RFDC_FOUND TRUE)
    else(RFDC_LIBRARY)
      message(FATAL_ERROR "RFdc library not found")
    endif(RFDC_LIBRARY)
  else(LIBMETAL_LIB)
    message(FATAL_ERROR "libmetal not found")
  endif(LIBMETAL_LIB)
endif(ENABLE_RFDC)

if(LIBIIO_FOUND OR RFDC_FOUND)
  set(RF_FOUND TRUE CACHE INTERNAL "RF frontend found")
else(LIBIIO_FOUND OR RFDC_FOUND)
  set(RF_FOUND FALSE CACHE INTERNAL "RF frontend found")
  add_definitions(-DDISABLE_RF)
endif(LIBIIO_FOUND OR RFDC_FOUND)

########################################################################
# Install Dirs
########################################################################
if (NOT CMAKE_INSTALL_LIBDIR)
  include(GNUInstallDirs)
endif (NOT CMAKE_INSTALL_LIBDIR)
# Fall back to just "lib" if the item provided by GNUInstallDirs doesn't exist
if (NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
  message(STATUS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} does not exist. Defaulting install location to ${CMAKE_INSTALL_PREFIX}/lib.")
  set(CMAKE_INSTALL_LIBDIR lib)
endif()

set(RUNTIME_DIR bin)
set(LIBRARY_DIR ${CMAKE_INSTALL_LIBDIR})
set(INCLUDE_DIR include)

########################################################################
# Compiler specific setup
########################################################################
macro(ADD_CXX_COMPILER_FLAG_IF_AVAILABLE flag have)
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(${flag} ${have})
  if(${have})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
  endif(${have})
endmacro(ADD_CXX_COMPILER_FLAG_IF_AVAILABLE)

macro(ADD_C_COMPILER_FLAG_IF_AVAILABLE flag have)
  include(CheckCCompilerFlag)
  check_c_compiler_flag(${flag} ${have})
  if(${have})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
  endif(${have})
endmacro(ADD_C_COMPILER_FLAG_IF_AVAILABLE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-comment -Wno-reorder -Wno-unused-variable -Wtype-limits -std=c++14 -fno-strict-aliasing")

  ADD_CXX_COMPILER_FLAG_IF_AVAILABLE("-Wno-unused-but-set-variable" HAVE_WNO_UNUSED_BUT_SET_VARIABLE)

  find_package(SSE)

  ADD_C_COMPILER_FLAG_IF_AVAILABLE("-march=${GCC_ARCH}" HAVE_MARCH_${GCC_ARCH})
  ADD_CXX_COMPILER_FLAG_IF_AVAILABLE("-march=${GCC_ARCH}" HAVE_MARCH_${GCC_ARCH})

  if (HAVE_AVX2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse -mavx2 -DLV_HAVE_AVX2 -DLV_HAVE_AVX -DLV_HAVE_SSE")
  else (HAVE_AVX2)
    if(HAVE_AVX)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse -mavx -DLV_HAVE_AVX -DLV_HAVE_SSE")
    elseif(HAVE_SSE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpmath=sse -msse4.1 -DLV_HAVE_SSE")
    endif(HAVE_AVX)
  endif (HAVE_AVX2)

  # Do not hide symbols in debug mode so backtraces can display function info.
  if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    if(NOT WIN32)
      ADD_CXX_COMPILER_FLAG_IF_AVAILABLE(-fvisibility=hidden HAVE_VISIBILITY_HIDDEN_CXX)
    endif(NOT WIN32)
  endif(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")

endif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")


ADD_C_COMPILER_FLAG_IF_AVAILABLE("-Werror=incompatible-pointer-types" HAVE_ERROR_INCOMPATIBLE)

if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")

  if (${CMAKE_CXX_COMPILER_VERSION} LESS 11)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-comment -Wno-write-strings -Wno-unused-result -Wformat -Wmissing-field-initializers -Wtype-limits -std=c99 -fno-strict-aliasing -D_GNU_SOURCE")
  else()   
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Warray-parameter=1 -Wall -Wno-comment -Wno-write-strings -Wno-unused-result -Wformat -Wmissing-field-initializers -Wtype-limits -std=c99 -fno-strict-aliasing -D_GNU_SOURCE")
  endif()

  ADD_C_COMPILER_FLAG_IF_AVAILABLE("-Wno-unused-but-set-variable" HAVE_WNO_UNUSED_BUT_SET_VARIABLE)
  if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -O0 -DDEBUG_MODE -DBUILD_TYPE_DEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -O0 -DDEBUG_MODE -DBUILD_TYPE_DEBUG")
  else(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    if(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -DBUILD_TYPE_RELWITHDEBINFO")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -DBUILD_TYPE_RELWITHDEBINFO")
    else(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fno-trapping-math -fno-math-errno -DBUILD_TYPE_RELEASE")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fno-trapping-math -fno-math-errno -DBUILD_TYPE_RELEASE")
    endif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
  endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

  find_package(SSE)

  if (HAVE_AVX2)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -mavx2 -DLV_HAVE_AVX2 -DLV_HAVE_AVX -DLV_HAVE_SSE")
  else (HAVE_AVX2)
    if(HAVE_AVX)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -mavx -DLV_HAVE_AVX -DLV_HAVE_SSE")
    elseif(HAVE_SSE)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse -msse4.1 -DLV_HAVE_SSE")
    endif(HAVE_AVX)
  endif (HAVE_AVX2)

  if (HAVE_FMA)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfma -DLV_HAVE_FMA")
  endif (HAVE_FMA)

  if (HAVE_AVX512)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx512f -mavx512cd -mavx512bw -mavx512dq -DLV_HAVE_AVX512")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512cd -mavx512bw -mavx512dq -DLV_HAVE_AVX512")
  endif(HAVE_AVX512)

  if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    if(HAVE_SSE)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -funroll-loops")
    endif(HAVE_SSE)
  endif(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")

  if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DIS_ARM -DHAVE_NEON")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIS_ARM")
    message(STATUS "Detected ARM processor")
    set(HAVE_NEON "True")
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
      set(HAVE_NEONv8 "True")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_NEONv8")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_NEONv8")
    endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard -mfpu=neon")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard -mfpu=neon")
    endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
  else(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set(HAVE_NEON "False")
  endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  set(CMAKE_REQUIRED_FLAGS ${CMAKE_C_FLAGS})

  if(NOT HAVE_SSE AND NOT HAVE_NEON AND NOT DISABLE_SIMD)
    message(FATAL_ERROR "no SIMD instructions found")
  endif(NOT HAVE_SSE AND NOT HAVE_NEON AND NOT DISABLE_SIMD)

  # Do not hide symbols in debug mode so backtraces can display function info.
  if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    if(NOT WIN32)
      ADD_C_COMPILER_FLAG_IF_AVAILABLE(-fvisibility=hidden HAVE_VISIBILITY_HIDDEN_C)
    endif(NOT WIN32)
  endif(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")

endif(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")

# Add -Werror to C/C++ flags for newer compilers
if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU")
  # Increase inlining limit to allow gcc compilation on e.g. RPi2
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --param large-function-growth=1600")
endif(CMAKE_C_COMPILER_ID MATCHES "GNU")

message(STATUS "CMAKE_C_FLAGS is ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS is ${CMAKE_CXX_FLAGS}")

########################################################################
# Add -fPIC property to all targets
########################################################################
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

########################################################################
# Print summary
########################################################################
message(STATUS "Using install prefix: ${CMAKE_INSTALL_PREFIX}")


########################################################################
# Add general includes and dependencies
########################################################################
include_directories(${PROJECT_BINARY_DIR}/lib/include)
include_directories(${PROJECT_SOURCE_DIR}/lib/include)
########################################################################
# Add the subdirectories
########################################################################
add_subdirectory(lib)