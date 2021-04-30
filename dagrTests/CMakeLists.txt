function(addDagrTest EXE_NAME SRC_NAME)
  add_executable(${EXE_NAME} ${SRC_NAME})
  buildWithHSA(${EXE_NAME})
  add_test(${EXE_NAME} ${EXE_NAME})
endfunction()

addDagrTest(createSignalCost createSignalCost.cpp)
addDagrTest(queryRegions queryRegions.cpp)
addDagrTest(batchLaunch batchLaunch.cpp)
addDagrTest(hipKernelTest hipKernelTest.cpp)
addDagrTest(treeDagLaunch treeDagLaunch.cpp)

# add_executable(queryMemPools queryMemPools.cpp)
# buildWithHSA(queryMemPools)

