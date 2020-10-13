Common stuff for building our tasking code with cmake

#  Add this as a git submodule to your repo 
https://git-scm.com/book/en/v2/Git-Tools-Submodules



#  Add it to your project's CMakeLists.txt

Put this in the CMakeLists.txt (near the top)
`include(cmakeUtils/common.cmake)`

# the above will set the CMAKE_MODULE_PATH to include cmakeUtils/Modules


Include other code using:

`include(cmakeUtils/file.cmake)`
