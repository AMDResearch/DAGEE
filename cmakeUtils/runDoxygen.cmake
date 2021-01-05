# README XXX
# Example usage, in main CMakeLists.txt:
# set(DOXYFILE_SOURCE_DIRS "\"${CMAKE_CURRENT_SOURCE_DIR}/docs ${CMAKE_CURRENT_SOURCE_DIR}/mySrdDir ...\"")
# include(cmakeUtils/runDoxygen.cmake) # i.e. this file

find_package(Doxygen)
if(DOXYGEN_FOUND)
  #TODO: double check the list of directories here
  configure_file(${PROJECT_SOURCE_DIR}/cmakeUtils/Doxyfile.in
    ${PROJECT_BINARY_DIR}/Doxyfile.in @ONLY)
  add_custom_target(doc ${DOXYGEN_EXECUTABLE}
     ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in 
     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
     COMMENT "Running Doxygen to generate documentation"
     VERBATIM)
else()
  message(STATUS "Install Doxygen to enable 'make doc' to generate documentation")
endif()
