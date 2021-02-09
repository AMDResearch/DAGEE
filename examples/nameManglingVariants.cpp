// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#include "dagee/ATMIalloc.h"
#include "dagee/ATMIdagExecutor.h"

#include <cassert>
#include <cstdlib>

#include <vector>

template <typename T>
__global__ void templatedKernel(T* ptr, size_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    ptr[i] = static_cast<T>(1);
  }
}

namespace myNmSpace {

template <typename T>
__global__ void templatedKernel(T* ptr, size_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    ptr[i] = static_cast<T>(1);
  }
}

struct MyClass {
  template <typename T>
  __global__ static void templatedKernel(T* ptr, size_t N) {
    size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    if (i < N) {
      ptr[i] = static_cast<T>(1);
    }
  }
};

template <unsigned NUM>
struct MyTmplClass {
  template <typename T>
  __global__ static void templatedKernel(T* ptr, size_t N) {
    size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
    if (i < N) {
      ptr[i] = static_cast<T>(1);
    }
  }
};
} // namespace myNmSpace

constexpr static const unsigned threadsPerBlock = 64;
constexpr static const unsigned blocks = 1;

template <typename E, typename F, typename... Args>
void testKernelSignature(E& gpuEx, F func, Args... args) {
  auto k = gpuEx.template registerKernel<Args...>(func);

  auto t = gpuEx.launchTask(gpuEx.makeTask(blocks, threadsPerBlock, k, args...));
  gpuEx.waitOnTask(t);
}

int main(int, char**) {
  constexpr size_t N = threadsPerBlock * blocks;

  std::vector<uint32_t> A(N, 0);
  std::vector<float> B(N, 0);
  std::vector<double> C(N, 0);

  std::cout << "info: copy Host2Device\n";

  using GpuExec = dagee::GpuExecutorAtmi;
  // using DagExec = dagee::ATMIdagExecutor<GpuExec>;

  dagee::AllocManagerAtmi bufMgr;

  auto A_d = bufMgr.makeDeviceCopy(A);
  auto B_d = bufMgr.makeDeviceCopy(B);
  auto C_d = bufMgr.makeDeviceCopy(C);

  GpuExec gpuEx;

  std::cout << "Executing tasks\n";

  testKernelSignature(gpuEx, &templatedKernel<uint32_t>, A_d, N);
  testKernelSignature(gpuEx, &templatedKernel<float>, B_d, N);
  testKernelSignature(gpuEx, &templatedKernel<double>, C_d, N);

  testKernelSignature(gpuEx, &myNmSpace::templatedKernel<uint32_t>, A_d, N);
  testKernelSignature(gpuEx, &myNmSpace::templatedKernel<float>, B_d, N);
  testKernelSignature(gpuEx, &myNmSpace::templatedKernel<double>, C_d, N);

  testKernelSignature(gpuEx, &myNmSpace::MyClass::templatedKernel<uint32_t>, A_d, N);
  testKernelSignature(gpuEx, &myNmSpace::MyClass::templatedKernel<float>, B_d, N);
  testKernelSignature(gpuEx, &myNmSpace::MyClass::templatedKernel<double>, C_d, N);

  testKernelSignature(gpuEx, &myNmSpace::MyTmplClass<0u>::templatedKernel<uint32_t>, A_d, N);
  testKernelSignature(gpuEx, &myNmSpace::MyTmplClass<1u>::templatedKernel<float>, B_d, N);
  testKernelSignature(gpuEx, &myNmSpace::MyTmplClass<2u>::templatedKernel<double>, C_d, N);

  std::cout << "info: copy Device2Host\n";
  bufMgr.copyBufferToVec(A, A_d);

  // checkOutput(A);

  return 0;
}
