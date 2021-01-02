// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#include "kiteDagGpu.h"

#include "dagee/ATMIdagExecutor.h"
#include "dagee/DeviceAlloc.h"

#include <cassert>
#include <cstdlib>

#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
  constexpr unsigned threadsPerBlock = 1024;
  constexpr unsigned blocks = 16;

  constexpr size_t N = threadsPerBlock * blocks;
  constexpr size_t NUM_ITERATIONS = 10;

  std::vector<uint32_t> A(N, 0);
  std::vector<uint32_t> B(N, 0);
  std::vector<uint32_t> C(N, 0);

  std::cout << "info: copy Host2Device\n";
  dagee::DeviceBufMgr dbuf;
  auto A_d = dbuf.makeDeviceCopy(A);
  auto B_d = dbuf.makeDeviceCopy(B);
  auto C_d = dbuf.makeDeviceCopy(C);

  using GpuExec = dagee::GpuExecutorAtmi;
  using DagExec = dagee::ATMIdagExecutor<GpuExec>;

  GpuExec gpuEx;
  DagExec dagEx(gpuEx);

  auto topK = gpuEx.registerKernel<uint32_t*, size_t>(&topKern);
  auto midK = gpuEx.registerKernel<uint32_t*, uint32_t*, size_t, uint32_t>(&midKern);
  auto bottomK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, size_t>(&bottomKern);

  auto* dag = dagEx.makeDAG();

  auto topTask = dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, topK, A_d, N));
  auto leftTask =
      dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, B_d, N, LEFT_ADD_VAL));
  auto rightTask =
      dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, C_d, N, RIGHT_ADD_VAL));
  auto bottomTask =
      dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, bottomK, A_d, B_d, C_d, N));

  dag->addEdge(topTask, leftTask);
  dag->addEdge(topTask, rightTask);
  dag->addEdge(leftTask, bottomTask);
  dag->addEdge(rightTask, bottomTask);

  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    dagEx.execute(dag);
  }

  std::cout << "info: copy Device2Host\n";
  dbuf.copyToHost(A, A_d);

  std::cout << "info: check result\n";
  bool passed = true;

  for (size_t i = 0; i < N; i++) {
    // std::printf("A[%zd] == %u\n", i, A[i]);
    if (A[i] != FINAL_VAL) {
      passed = false;
    }
  }

  if (!passed) {
    CHECK(hipErrorUnknown);
  } else {
    std::cout << "PASSED!\n";
  }
}
