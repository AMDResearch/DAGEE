# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

add_executable(kiteDagGpu kiteDagGpu.cpp)
buildWithATMI(kiteDagGpu)

add_executable(kiteDagCpu kiteDagCpu.cpp)
buildWithATMI(kiteDagCpu)

add_executable(kiteDagMixed kiteDagMixed.cpp)
buildWithATMI(kiteDagMixed)

add_executable(kiteDagInLoop kiteDagInLoop.cpp)
buildWithATMI(kiteDagInLoop)

add_executable(atmiDenq atmiDenq.cpp)
buildWithATMI(atmiDenq)
