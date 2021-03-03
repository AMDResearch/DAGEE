# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#include order matters
defineCMakeVar(ROCM_ROOT /opt/rocm)
include(cmakeUtils/rocmVersion.cmake)
include(cmakeUtils/addHSA.cmake)
include(cmakeUtils/addATMI.cmake)
