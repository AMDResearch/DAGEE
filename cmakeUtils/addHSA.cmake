# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

defineCMakeVar(HSA_ROOT ${ROCM_ROOT})

find_package(HSA)

function(buildWithHSA target) 
  if(NOT HSA_FOUND) 
    message(FATAL_ERROR "HSA not found, Required library")
  endif()

  target_include_directories(${target} PUBLIC ${HSA_INCLUDE_DIRS})

  if (${ARGC} GREATER 1) 
    set(ADD_RPATH ${ARGV1}) #ARGV numbering starts at 0
  endif()

  if(ADD_RPATH) 
    list(GET HSA_LIBRARIES 0 FIRST_HSA_LIB)
    # message(STATUS "FIRST_HSA_LIB=${FIRST_HSA_LIB}")
    get_filename_component(HSA_RPATH ${FIRST_HSA_LIB} DIRECTORY CACHE)
    linkLibWithRpath(${target} ${HSA_RPATH} ${HSA_LIBRARIES})
    # message(STATUS "HSA_RPATH=${HSA_RPATH}")
  else()
    target_link_libraries(${target} PUBLIC ${HSA_LIBRARIES})
  endif()
endfunction()
