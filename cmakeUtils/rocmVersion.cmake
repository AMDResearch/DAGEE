# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

# provides cmake variables and compiler definitions for rocm versions

defineCMakeVar(ROCM_ROOT /opt/rocm)

if(NOT ROCM_VERSION)
  file(GLOB version_files
      LIST_DIRECTORIES false
      ${ROCM_ROOT}/.info/version*
      )
  list(GET version_files 0 version_file)
  # Compute the version
  execute_process(
      COMMAND cat ${version_file}
      OUTPUT_VARIABLE _rocm_version
      ERROR_VARIABLE _rocm_error
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
      )
  if(NOT _rocm_error)
    set(ROCM_VERSION ${_rocm_version} CACHE STRING "Version of ROCm as found in ${ROCM_ROOT}/.info/version*")
  else()
    set(ROCM_VERSION "0.0.0" CACHE STRING "Version of ROCm set to default")
  endif()
  mark_as_advanced(ROCM_VERSION)
endif()

string(REPLACE "." ";"  _rocm_version_list "${ROCM_VERSION}")
list(GET _rocm_version_list 0 ROCM_VERSION_MAJOR)
list(GET _rocm_version_list 1 ROCM_VERSION_MINOR)
list(GET _rocm_version_list 2 ROCM_VERSION_PATCH)
set(ROCM_VERSION_STRING "${ROCM_VERSION}")

mark_as_advanced(
      ROCM_VERSION
      ROCM_VERSION_MAJOR
      ROCM_VERSION_MINOR
      ROCM_VERSION_PATCH
      ROCM_VERSION_STRING
)

add_definitions(
  -DROCM_VERSION=${ROCM_VERSION}
  -DROCM_VERSION_MAJOR=${ROCM_VERSION_MAJOR}
  -DROCM_VERSION_MINOR=${ROCM_VERSION_MINOR}
  -DROCM_VERSION_PATCH=${ROCM_VERSION_PATCH}
  -DROCM_VERSION_STRING=${ROCM_VERSION_STRING}
  )
