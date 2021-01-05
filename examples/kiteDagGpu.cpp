// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#include "kiteDagGpu.h"

#include "dagee/ATMIalloc.h"
#include "dagee/ATMIdagExecutor.h"

#include <cassert>
#include <cstdlib>

#include <vector>

template <bool LAUNCH_KERNELS_INDIVIDUALLY>
void kiteDag() {
  constexpr unsigned threadsPerBlock = 1024;
  constexpr unsigned blocks = 16;

  constexpr size_t N = threadsPerBlock * blocks;

  std::vector<uint32_t> A(N, 0);
  std::vector<uint32_t> B(N, 0);
  std::vector<uint32_t> C(N, 0);

  std::cout << "info: copy Host2Device\n";

  using GpuExec = dagee::GpuExecutorAtmi;
  using DagExec = dagee::ATMIdagExecutor<GpuExec>;


  dagee::AllocManagerAtmi dbuf;

  auto A_d = dbuf.makeDeviceCopy(A);
  auto B_d = dbuf.makeDeviceCopy(B);
  auto C_d = dbuf.makeDeviceCopy(C);

  //![Eager Launch]

  GpuExec gpuEx;

  auto topK = gpuEx.registerKernel<uint32_t*, size_t>(&topKern);
  auto midK = gpuEx.registerKernel<uint32_t*, uint32_t*, size_t, uint32_t>(&midKern);
  auto bottomK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, size_t>(&bottomKern);

  if (LAUNCH_KERNELS_INDIVIDUALLY) {
    std::cout << "Launching kernels individually\n";

    auto topTask = gpuEx.launchTask(gpuEx.makeTask(blocks, threadsPerBlock, topK, A_d, N));

    // these two can go in parallel
    auto leftTask = gpuEx.launchTask(
        gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, B_d, N, LEFT_ADD_VAL), {topTask});

    auto rightTask = gpuEx.launchTask(
        gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, C_d, N, RIGHT_ADD_VAL), {topTask});

    auto bottomTask = gpuEx.launchTask(
        gpuEx.makeTask(blocks, threadsPerBlock, bottomK, A_d, B_d, C_d, N), {leftTask, rightTask});
    gpuEx.waitOnTask(bottomTask);
  //![Eager Launch]

  } else {
    std::cout << "Building Kite DAG\n";

    DagExec dagEx(gpuEx);
    auto* dag = dagEx.makeDAG();

    auto topTask = dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, topK, A_d, N));
    auto leftTask =
        dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, B_d, N, LEFT_ADD_VAL));
    auto rightTask =
        dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, C_d, N, RIGHT_ADD_VAL));
    auto bottomTask =
        dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, bottomK, A_d, B_d, C_d, N));

    dag->addFanOutEdges(topTask, {rightTask, leftTask});
    dag->addFanInEdges({rightTask, leftTask}, bottomTask);

    std::cout << "Executing Kite DAG\n";

    dagEx.execute(dag);
  }

  std::cout << "info: copy Device2Host\n";
  dbuf.copyBufferToVec(A, A_d);

  checkOutput(A);
}

int main(int argc, char* argv[]) {
  kiteDag<true>();
  kiteDag<false>();
}
