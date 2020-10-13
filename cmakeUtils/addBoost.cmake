# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

find_package(Boost)
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found, Required library")
else()
  include_directories(${Boost_INCLUDE_DIRS})
endif()
