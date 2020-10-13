// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef ATMI_KITE_DAG_H
#define ATMI_KITE_DAG_H

#include "hip/hip_runtime.h"

#include <cstdio>
#include <cstdlib>

#include <iostream>

#define CHECK(cmd)                                                                                 \
    {                                                                                              \
        hipError_t error = cmd;                                                                    \
        if (error != hipSuccess) {                                                                 \
          fprintf(stderr, "error: '%s'(%d) at %s:%d\n", hipGetErrorString(error), error,           \
              __FILE__, __LINE__);                                                                 \
          std::abort();                                                                            \
        }                                                                                          \
    }

constexpr uint32_t INIT_VAL = 1;
constexpr uint32_t LEFT_ADD_VAL = 2;
constexpr uint32_t RIGHT_ADD_VAL = 3;
constexpr uint32_t FINAL_VAL = 3 * INIT_VAL + LEFT_ADD_VAL + RIGHT_ADD_VAL;


__global__ void topKern(uint32_t* A_d, size_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    A_d[i] = INIT_VAL;
  }
}

__global__ void midKern(uint32_t* A_d, uint32_t* B_d, size_t N, uint32_t addVal) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    B_d[i] = A_d[i] + addVal;
  }
}

__global__ void bottomKern(uint32_t* A_d, uint32_t*  B_d, uint32_t* C_d, size_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    A_d[i] = A_d[i] + B_d[i] + C_d[i];
  }
}

template <typename V>
void checkOutput(const V& A) {
  std::cout << "info: check result\n";
  bool passed = true;

  for (size_t i = 0; i < A.size(); i++) {
    // std::printf("A[%zd] == %u\n", i, A[i]);
    if (A[i] != FINAL_VAL) {
      std::printf("Failed at A[%zd] = %u | Expected = %u\n", i, A[i], FINAL_VAL);
      passed = false;
    }
  }

  if (!passed) {
    CHECK(hipErrorUnknown);
  } else {
    std::cout << "PASSED!\n";
  }
}


#endif// ATMI_KITE_DAG_H
