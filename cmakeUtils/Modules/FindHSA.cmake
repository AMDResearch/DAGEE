# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

# Find HSA libraries
# Once done this will define
#  HSA_FOUND - System has HSA
#  HSA_INCLUDE_DIRS - The HSA include directories
#  HSA_LIBRARIES - The libraries needed to use HSA

if(HSA_INCLUDE_DIRS AND HSA_LIBRARIES)
  set(HSA_FIND_QUIETLY TRUE)
endif()

find_path(HSA_INCLUDE_DIRS hsa/hsa.h HINTS ${HSA_ROOT} ENV HSA_RUNTIME_PATH
  PATH_SUFFIXES include)
message(STATUS "HSA_INCLUDE_DIRS: ${HSA_INCLUDE_DIRS}")
find_library(ROCR_LIB NAMES hsa-runtime64 HINTS ${HSA_ROOT} ENV HSA_RUNTIME_PATH
  PATH_SUFFIXES lib lib64)
find_library(ROCT_LIB NAMES hsakmt HINTS ${HSA_ROOT} ENV HSA_RUNTIME_PATH
  PATH_SUFFIXES lib lib64)
set(HSA_LIBRARIES ${ROCR_LIB} ${ROCT_LIB})

message(STATUS "HSA_LIBRARIES: ${HSA_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HSA DEFAULT_MSG HSA_LIBRARIES HSA_INCLUDE_DIRS)

mark_as_advanced(HSA_INCLUDE_DIRS HSA_LIBRARIES)
