// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

/**
---------------------
Important
---------------------
To compile (specifically to link) Device-Enque code, such as atmiDenq, steps are:
1. Install aomp package from ROCm repos
2. Build atmi with -DATMI_DEVICE_RUNTIME=On
```
cmake -DLLVM_DIR=/opt/rocm/aomp -DATMI_DEVICE_RUNTIME=On <path-to-atmi-src-dir>
```

3. Before running make for this example , export the following environment
   variable (replace GFX900 with the architectural codename for your GPU. See
`/opt/rocm/bin/rocminfo | grep Name`):

```
export HCC_EXTRA_LIBRARIES_GFX900=<ATMI_INSTALL_OR_BUILD_PATH>/lib/atmi-gfx900.amdgcn.bc
```

or

```
HCC_EXTRA_LIBRARIES=<ATMI_INSTALL_OR_BUILD_PATH>/lib/atmi-gfx900.amdgcn.bc make atmiDenq
```

*/

#include "dagee/ATMIdagExecutor.h"
#include "dagee/DeviceAlloc.h"

#include "hip/hcc_detail/program_state.hpp"
#include "hip/hip_runtime.h"

#include "hsa/hsa.h"

#include <cassert>
#include <cstdlib>

#include <iostream>
#include <vector>
__device__ extern "C" void atmid_task_launch(atmi_lparm_t* lp, uint64_t kernel_id,
                                             void* args_region, size_t args_region_size);
#define CHECK(cmd)                                                                             \
  {                                                                                            \
    hipError_t error = cmd;                                                                    \
    if (error != hipSuccess) {                                                                 \
      fprintf(stderr, "error: '%s'(%d) at %s:%d\n", hipGetErrorString(error), error, __FILE__, \
              __LINE__);                                                                       \
      std::abort();                                                                            \
    }                                                                                          \
  }

enum { K_ID_topKern = 0, K_ID_leftKern, K_ID_rightKern, K_ID_bottomKern };

constexpr uint32_t INIT_VAL = 1;
constexpr uint32_t LEFT_ADD_VAL = 2;
constexpr uint32_t RIGHT_ADD_VAL = 3;
constexpr uint32_t FINAL_VAL = 3 * INIT_VAL + LEFT_ADD_VAL + RIGHT_ADD_VAL;

__global__ uint32_t d_flag = 0;

struct Args {
  uint32_t* first_op;
  uint32_t* second_op;
  uint32_t* third_op;
  uint64_t N;
};

__device__ void set_lparm(atmi_lparm_t* lparm) {
  lparm->groupDim[0] = hipBlockDim_x;
  lparm->groupDim[1] = hipBlockDim_y;
  lparm->groupDim[2] = hipBlockDim_z;
  lparm->gridDim[0] = hipGridDim_x * hipBlockDim_x;
  lparm->gridDim[1] = hipGridDim_y * hipBlockDim_y;
  lparm->gridDim[2] = hipGridDim_z * hipBlockDim_z;
  // lparm->group = 0ull;
  // lparm->groupable = true;
  lparm->synchronous = false;
  lparm->acquire_scope = ATMI_FENCE_SCOPE_SYSTEM;
  lparm->release_scope = ATMI_FENCE_SCOPE_SYSTEM;
  lparm->num_required = 0;
  lparm->requires = NULL;
  lparm->place = (atmi_place_t)ATMI_PLACE_GPU(0, 0);
  // default case for kernel enqueue: lparm->groupable = ATMI_TRUE;
}

__global__ void topKern(uint32_t* A_d, uint32_t* B_d, uint32_t* C_d, uint64_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    A_d[i] = INIT_VAL;
    B_d[i] = 0;
    C_d[i] = 0;
  }

  // launch leftKern and rightKern
  if (i == 0) {
    atmi_lparm_t lparm;
    set_lparm(&lparm);

    Args args;
    args.first_op = A_d;
    args.second_op = B_d;
    args.third_op = C_d;
    args.N = N;

    // launch left
    atmid_task_launch(&lparm, K_ID_leftKern, reinterpret_cast<void*>(&args), sizeof(Args));
    // launch right
    atmid_task_launch(&lparm, K_ID_rightKern, reinterpret_cast<void*>(&args), sizeof(Args));
  }
}

__global__ void leftKern(uint32_t* A_d, uint32_t* B_d, uint32_t* C_d, uint64_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    B_d[i] = A_d[i] + LEFT_ADD_VAL;
  }

  if (i == 0) {
    if (atomicAdd(&d_flag, 1) == 1) {
      // Right has already executed, so we can now launch bottom
      atmi_lparm_t lparm;
      set_lparm(&lparm);

      Args args;
      args.first_op = A_d;
      args.second_op = B_d;
      args.third_op = C_d;
      args.N = N;
      atmid_task_launch(&lparm, K_ID_bottomKern, reinterpret_cast<void*>(&args), sizeof(Args));
    }
  }
}

__global__ void rightKern(uint32_t* A_d, uint32_t* B_d, uint32_t* C_d, uint64_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    C_d[i] = A_d[i] + RIGHT_ADD_VAL;
  }

  if (i == 0) {
    if (atomicAdd(&d_flag, 1) == 1) {
      // Left has already executed, so we can now launch bottom
      atmi_lparm_t lparm;
      set_lparm(&lparm);

      Args args;
      args.first_op = A_d;
      args.second_op = B_d;
      args.third_op = C_d;
      args.N = N;
      atmid_task_launch(&lparm, K_ID_bottomKern, reinterpret_cast<void*>(&args), sizeof(Args));
    }
  }
}

__global__ void bottomKern(uint32_t* A_d, uint32_t* B_d, uint32_t* C_d, uint64_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    A_d[i] = A_d[i] + B_d[i] + C_d[i];
  }
  /*
  if (i == 0) {
    atomicAdd(&d_flag, 1); // bump d_flag up to 3 to signal completion
  }
  */
}

int main(int argc, char* argv[]) {
  constexpr unsigned threadsPerBlock = 1024;
  constexpr unsigned blocks = 16;

  constexpr uint64_t N = threadsPerBlock * blocks;

  std::cout << "WARNING: Please read the build instructions in comments. Otherwise, the program "
               "will build but not run"
            << std::endl;

  std::vector<uint32_t> A(N, 0);
  std::vector<uint32_t> B(N, 0);
  std::vector<uint32_t> C(N, 0);

  std::cout << "info: copy Host2Device\n";

  dagee::DeviceBufMgr dbuf;
  auto A_d = dbuf.makeDeviceCopy(A);
  auto B_d = dbuf.makeDeviceCopy(B);
  auto C_d = dbuf.makeDeviceCopy(C);

  dagee::GpuExecutorAtmi gpuEx;

  std::cout << "info: make kernels\n";

  auto topK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, uint64_t>(&topKern);
  auto leftK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, uint64_t>(&leftKern);
  auto rightK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, uint64_t>(&rightKern);
  auto bottomK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, uint64_t>(&bottomKern);

  std::cout << "Running kernels using device enqueue\n";

  auto topTask = gpuEx.launchTask(gpuEx.makeTask(blocks, threadsPerBlock, topK, A_d, B_d, C_d, N));
  gpuEx.waitOnTask(topTask);

  std::cout << "info: copy Device2Host\n";
  dbuf.copyToHost(A, A_d);

  std::cout << "info: check result\n";
  bool passed = true;

  for (size_t i = 0; i < N; i++) {
    if (A[i] != FINAL_VAL) {
      std::printf("A[%zd] %u != %u\n", i, A[i], FINAL_VAL);
      passed = false;
    }
  }

  if (!passed) {
    CHECK(hipErrorUnknown);
  } else {
    std::cout << "PASSED!\n";
  }
}
