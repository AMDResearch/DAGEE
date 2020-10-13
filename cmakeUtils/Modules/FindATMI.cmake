# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

# Find ATMI libraries
# Once done this will define
#  ATMI_FOUND - System has ATMI
#  ATMI_INCLUDE_DIRS - The ATMI include directories
#  ATMI_LIBRARIES - The libraries needed to use ATMI

if(ATMI_INCLUDE_DIRS AND ATMI_LIBRARIES)
  set(ATMI_FIND_QUIETLY TRUE)
endif()

find_path(ATMI_INCLUDE_DIRS atmi.h atmi_runtime.h HINTS ${ATMI_ROOT} ENV ATMI_RUNTIME_PATH
  PATH_SUFFIXES include)
find_library(ATMI_LIBRARIES NAMES atmi_runtime HINTS ${ATMI_ROOT} ENV ATMI_RUNTIME_PATH
  PATH_SUFFIXES lib lib64)

# message(STATUS "ATMI_INCLUDE_DIRS: ${ATMI_INCLUDE_DIRS}")
# message(STATUS "ATMI_LIBRARIES: ${ATMI_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ATMI DEFAULT_MSG ATMI_LIBRARIES ATMI_INCLUDE_DIRS)

mark_as_advanced(ATMI_INCLUDE_DIRS ATMI_LIBRARIES)
