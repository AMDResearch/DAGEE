// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_DUMMY_KERNEL_H
#define DAGEE_INCLUDE_DAGEE_DUMMY_KERNEL_H

#include "dagee/DeviceAlloc.h"

#include "hip/hip_common.h"
#include "hip/hip_runtime.h"

namespace dagee {
template <typename __UNUSED=void>
__global__ void dummyKernel(int* ptr) {
  ptr[hipThreadIdx_x] = hipThreadIdx_x + 1;
}

template <typename __UNUSED=void>
void launchDummyKernel(void) {
  int* ptr;
  deviceAlloc(ptr, 32);

  hipLaunchKernelGGL(&dummyKernel, dim3(1), dim3(32), 0, 0, ptr);

  deviceFree(ptr);
}

}// end namespace dagee
#endif// DAGEE_INCLUDE_DAGEE_DUMMY_KERNEL_H
