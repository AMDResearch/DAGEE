# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

function(addDageeTarget EXE_NAME SRC_NAME)
  add_executable(${EXE_NAME} ${SRC_NAME})
  buildWithATMI(${EXE_NAME})
  add_test(${EXE_NAME} ${EXE_NAME})
endfunction()

addDageeTarget(kiteDagSimple kiteDagSimple.cpp)
addDageeTarget(kiteDagGpu kiteDagGpu.cpp)
addDageeTarget(kiteDagCpu kiteDagCpu.cpp)
addDageeTarget(kiteDagMixed kiteDagMixed.cpp)
addDageeTarget(kiteDagInLoop kiteDagInLoop.cpp)
addDageeTarget(nameManglingVariants nameManglingVariants.cpp)

# FIXME TODO(amber) commenting out for now because it doesn't work with ROCm 4.0
# add_executable(atmiDenq atmiDenq.cpp)
# buildWithATMI(atmiDenq)
