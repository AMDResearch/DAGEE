# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

# Find PLASMA libraries
# Once done this will define
#  PLASMA_FOUND - System has PLASMA
#  PLASMA_INCLUDE_DIRS - The PLASMA include directories
#  PLASMA_LIBRARIES - The libraries needed to use PLASMA

if(PLASMA_INCLUDE_DIRS AND PLASMA_LIBRARIES)
  set(PLASMA_FIND_QUIETLY TRUE)
endif()

find_path(PLASMA_INCLUDE_DIRS cblas.h core_blas.h plasma.h lapacke.h HINTS ${PLASMA_ROOT} ENV PLASMA_ROOT PATH_SUFFIXES include)

# foreach(L libplasma.a libcoreblas.a liblapack.a libcoreblasqw.a libtmg.a liblapacke.a)
foreach(L libplasma.a libcoreblas.a libtmg.a liblapacke.a)
  find_library(PLASMA_LIB_${L} NAMES ${L} PATHS ${PLASMA_ROOT})
  set(PLASMA_LIBRARIES ${PLASMA_LIBRARIES} ${PLASMA_LIB_${L}})
endforeach()

set(PLASMA_LIBRARIES ${PLASMA_LIBRARIES} pthread hwloc m)

# message(STATUS "PLASMA_INCLUDE_DIRS: ${PLASMA_INCLUDE_DIRS}")
# message(STATUS "PLASMA_LIBRARIES: ${PLASMA_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PLASMA DEFAULT_MSG PLASMA_LIBRARIES PLASMA_INCLUDE_DIRS)

mark_as_advanced(PLASMA_INCLUDE_DIRS PLASMA_LIBRARIES)
