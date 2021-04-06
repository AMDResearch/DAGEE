# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

function(addDageeTarget EXE_NAME SRC_NAME)
  add_executable(${EXE_NAME} ${SRC_NAME})
  buildWithATMI(${EXE_NAME})
  add_test(${EXE_NAME} ${EXE_NAME})
endfunction()

addDageeTarget(kiteDagSimple kiteDagSimple.cpp)
addDageeTarget(kiteDagGpu kiteDagGpu.cpp)
addDageeTarget(kiteDagGpuNoAuto kiteDagGpuNoAuto.cpp)
addDageeTarget(kiteDagCpu kiteDagCpu.cpp)
addDageeTarget(kiteDagMixed kiteDagMixed.cpp)
addDageeTarget(kiteDagMixedNoAuto kiteDagMixedNoAuto.cpp)
addDageeTarget(kiteDagInLoop kiteDagInLoop.cpp)
addDageeTarget(nameManglingVariants nameManglingVariants.cpp)
addDageeTarget(atmiDenq atmiDenq.cpp)

set_property(TARGET atmiDenq PROPERTY COMPILE_FLAGS "-Xclang -mlink-builtin-bitcode -Xclang ${ATMI_ROOT}/lib/atmi-${GFX_VER}.bc")
