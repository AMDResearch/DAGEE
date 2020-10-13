# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

# Find PARSEC libraries
# Once done this will define
#  PARSEC_FOUND - System has PARSEC
#  PARSEC_INCLUDE_DIRS - The PARSEC include directories
#  PARSEC_LIBRARIES - The libraries needed to use PARSEC

if(PARSEC_INCLUDE_DIRS AND PARSEC_LIBRARIES)
  set(PARSEC_FIND_QUIETLY TRUE)
endif()

find_path(PARSEC_INCLUDE_DIRS dague.h dague_config.h dplasma.h HINTS ${PARSEC_ROOT} ENV PARSEC_ROOT PATH_SUFFIXES include)

foreach(L libdague.a libdague-base.a libdplasma.a libdague_distribution_matrix.a libdague_distribution.a)
  find_library(PARSEC_LIB_${L} NAMES ${L} PATHS ${PARSEC_ROOT})
  set(PARSEC_LIBRARIES ${PARSEC_LIBRARIES} ${PARSEC_LIB_${L}})
endforeach()

set(PARSEC_LIBRARIES ${PARSEC_LIBRARIES} pthread hwloc m)

# message(STATUS "PARSEC_INCLUDE_DIRS: ${PARSEC_INCLUDE_DIRS}")
# message(STATUS "PARSEC_LIBRARIES: ${PARSEC_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PARSEC DEFAULT_MSG PARSEC_LIBRARIES PARSEC_INCLUDE_DIRS)

mark_as_advanced(PARSEC_INCLUDE_DIRS PARSEC_LIBRARIES)
