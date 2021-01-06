# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

include(cmakeUtils/addHSA.cmake)
defineCMakeVar(AMD_LLVM /opt/rocm/aomp)
defineCMakeVar(GFX_VER gfx900)
# defineCMakeVar(ROCM_DEVICE_LIBS /opt/rocm/opencl/lib/x86_64/bitcode)
# defineCMakeVar(ROCM_DEVICE_LIBS ${AMD_LLVM})

defineCMakeVar(ATMI_SRC ${CMAKE_SOURCE_DIR}/atmi-staging)
if (EXISTS ${ATMI_SRC} AND NOT ${AMIT_ROOT})
  set(ATMI_BUILD_FROM_SRC TRUE)
endif()

defineCMakeVar(ATMI_ROOT /opt/rocm/atmi) # default to submodule dir

if(ATMI_BUILD_FROM_SRC)
  # setting up ATMI's CMake options
  set(LLVM_DIR ${AMD_LLVM})
  # set(DEVICE_LIB_DIR ${ROCM_DEVICE_LIBS})
  set(ATMI_DEVICE_RUNTIME ON)
  # invoke src/CMakeLists.txt
  add_subdirectory(${ATMI_SRC}/src)
  # set variables for apps to use ATMI
  set(ATMI_INCLUDE_DIRS ${ATMI_SRC}/include)
  set(ATMI_LIBRARIES atmi_runtime) # same lib name as found in ${ATMI_SRC}/runtime/core/CMakeLists.txt
else()
  # if ATMI_ROOT is defined at cmake cmd line, we use find_package to set
  # include dir & lib etc.
  find_package(ATMI)
  if(NOT ATMI_FOUND) 
    message(FATAL_ERROR "ATMI not found, Required library")
  endif()
endif()

function(buildWithATMI target) 
  target_include_directories(${target} PUBLIC ${ATMI_INCLUDE_DIRS})
  target_include_directories(${target} PUBLIC ${CMAKE_SOURCE_DIR}/atmiWrap/include)
  if (${ARGC} GREATER 1) 
    set(ADD_RPATH ${ARGV1}) #ARGV numbering starts at 0
    # message(STATUS "ADD_RPATH=${ADD_RPATH}")
  endif()

  if(ADD_RPATH)
    if (TARGET ${ATMI_LIBRARIES}) 
      get_property(ATMI_RPATH TARGET ${ATMI_LIBRARIES} PROPERTY LIBRARY_OUTPUT_DIRECTORY)
    else()
      get_filename_component(ATMI_RPATH ${ATMI_LIBRARIES} DIRECTORY CACHE)
    endif()
  endif()

  if(ADD_RPATH)
    # message(STATUS "ATMI_RPATH=${ATMI_RPATH}")
    linkLibWithRpath(${target} ${ATMI_RPATH} ${ATMI_LIBRARIES})
  else()
    target_link_libraries(${target} PUBLIC ${ATMI_LIBRARIES})
  endif()
  buildWithHSA(${target} ${ADD_RPATH})
endfunction()

function(compileCL2GCN target clfile)

  set(CLOC ${ATMI_ROOT}/bin/cloc.sh)

  cmake_parse_arguments(
    CLOC # prefix of output variables
    "" # list of names of the boolean arguments (only defined ones will be true)
    "NOT_USED" # list of names of mono-valued arguments
    # "INC_DIRS;H_DEPS" # list of names of multi-valued arguments (output variables are lists)
    "H_DEPS" # list of names of multi-valued arguments (output variables are lists)
    ${ARGN} # arguments of the function to parse, here we take the all unnamed ones
    )

  set(ATMI_WRAP_DIR ${CMAKE_SOURCE_DIR}/atmiWrap/include)

  # dummmKernel.h is defined in atmiWrap/include
  list(APPEND CLOC_H_DEPS ${ATMI_WRAP_DIR}/dummyKernel.h ${ATMI_WRAP_DIR}/hipToCLdef.h)

  # message(STATUS "CLOC_H_DEPS: ${CLOC_H_DEPS}")
  # set(CLOC_IMPLICIT_DEPS "")
  # foreach(f ${CLOC_H_DEPS})
    # list(APPEND CLOC_IMPLICIT_DEPS C ${f})
  # endforeach()

  set(CLOC_INC_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${ATMI_WRAP_DIR})

  set(CL_INC_DIR_FLAGS "")
  foreach(d ${CLOC_INC_DIRS})
    set(CL_INC_DIR_FLAGS "${CL_INC_DIR_FLAGS} -I ${d}")
  endforeach()

  set(AMDGPU_TARGET_TRIPLE amdgcn-amd-amdhsa) 
  set(CL_OPTS  "${CL_OPTS} -O2 -v ${CL_INC_DIR_FLAGS}")
  # set(CLOC_OPTS -vv -opt 2 -aomp ${AMD_LLVM} -triple ${AMDGPU_TARGET_TRIPLE} -mcpu ${GFX_VER} -libgcn ${ROCM_DEVICE_LIBS} -atmipath ${ATMI_ROOT})
  set(CLOC_OPTS -vv -opt 2 -aomp ${AMD_LLVM} -triple ${AMDGPU_TARGET_TRIPLE} -mcpu ${GFX_VER} -atmipath ${ATMI_ROOT})


  set(OUTFILE ${clfile}.hsaco)
  add_custom_target(${target} DEPENDS ${OUTFILE})
  add_custom_command(OUTPUT ${OUTFILE} 
    COMMAND ${CLOC} ${CLOC_OPTS} -clopts " ${CL_OPTS} " -o ${OUTFILE} ${CMAKE_CURRENT_SOURCE_DIR}/${clfile}
    DEPENDS ${clfile} ${CLOC_H_DEPS}
    COMMENT "Building ${clfile} using ${CLOC}"
    VERBATIM)

endfunction()
