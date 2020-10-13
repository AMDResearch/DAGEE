# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmakeUtils/Modules/")

macro(defineCMakeVar VARNAME DEFAULT_VAL)
  if(NOT ${VARNAME})
    set(${VARNAME} ${DEFAULT_VAL})
    message(STATUS "${VARNAME} set to ${${VARNAME}}. To override, run cmake with -D${VARNAME}=BLAH")
  endif()
endmacro()

function(linkLibWithRpath MY_TARGET MY_RPATH MY_LIBS)
  target_link_libraries(${MY_TARGET} PRIVATE -Wl,-R,${MY_RPATH} ${MY_LIBS})
endfunction()
