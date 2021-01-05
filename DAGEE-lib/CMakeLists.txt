cmake_minimum_required(VERSION 3.1)

project(DAGEE-lib)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(CMAKE_CXX_STANDARD 11) # C++11...
set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...
set(CMAKE_CXX_EXTENSIONS OFF) #...without compiler extensions like gnu++11

# set(CMAKE_EXE_LINKER_FLAGS "-static")


# find_package(Boost)
# if(NOT Boost_FOUND)
  # message(FATAL_ERROR "Boost not found, Required library")
# else()
  # include_directories(${Boost_INCLUDE_DIRS})
# endif()


include_directories(include)

# function(app name source)
  # add_executable(${name} ${source})
  # target_link_libraries(${name} ${Boost_LIBRARIES})
  # # target_link_libraries(${name} pthread)
# 
# endfunction()
# 
# # add_subdirectory(apps)
# add_subdirectory(tests)
# 
