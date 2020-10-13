// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_NULL_KERNEL_H
#define DAGEE_INCLUDE_DAGEE_NULL_KERNEL_H

#include "hip/hip_common.h"
#include "hip/hip_runtime.h"

namespace dagee {
template <typename __UNUSED=void>
__global__ void nullKernel(int* ptr) {
  return;
}
}// end namespace dagee

#endif// DAGEE_INCLUDE_DAGEE_NULL_KERNEL_H
