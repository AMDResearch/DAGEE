# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" COMPILER_ID)

add_compile_options(-Wall)
if(${COMPILER_ID} MATCHES "gnu") 
  add_compile_options(-Wextra -Wpedantic)
elseif(${COMPILER_ID} MATCHES "intel")
  add_compile_options(-Wcheck)
elseif(${COMPILER_ID} MATCHES "clang")
  # add_compile_options(-Weverything)
  # add_compile_options(-Wall)
endif()

