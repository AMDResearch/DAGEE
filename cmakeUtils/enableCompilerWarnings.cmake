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

set(GCC_NO_PIE_FLAGS "-no-pie -m64")
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${GCC_NO_PIE_FLAGS}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_NO_PIE_FLAGS}")
endif()

if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
  set(CMAKE_F_FLAGS  "${CMAKE_F_FLAGS} ${GCC_NO_PIE_FLAGS}")
endif()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
  string(REGEX REPLACE "\<-O[0-9]\>" "-O0" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REGEX REPLACE "\<-O[0-9]\>" "-O0" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
  message(STATUS "CMAKE_CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
else ()
  message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
endif()
