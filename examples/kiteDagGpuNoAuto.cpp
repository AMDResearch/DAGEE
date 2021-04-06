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

  dagee::AllocManagerAtmi bufMgr;

  uint32_t* A_d = bufMgr.makeDeviceCopy(A);
  uint32_t* B_d = bufMgr.makeDeviceCopy(B);
  uint32_t* C_d = bufMgr.makeDeviceCopy(C);

  //![Eager Launch]

  GpuExec gpuEx;

  using KernelInfo = GpuExec::KernelInfo;

  KernelInfo topK = gpuEx.registerKernel<uint32_t*, size_t>(&topKern);
  KernelInfo midK = gpuEx.registerKernel<uint32_t*, uint32_t*, size_t, uint32_t>(&midKern);
  KernelInfo bottomK = gpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, size_t>(&bottomKern);

  if (LAUNCH_KERNELS_INDIVIDUALLY) {
    std::cout << "Launching kernels individually\n";

    dagee::ATMItaskHandle topTask = gpuEx.launchTask(gpuEx.makeTask(blocks, threadsPerBlock, topK, A_d, N));

    // these two can go in parallel
    dagee::ATMItaskHandle leftTask = gpuEx.launchTask(
        gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, B_d, N, LEFT_ADD_VAL), {topTask});

    dagee::ATMItaskHandle rightTask = gpuEx.launchTask(
        gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, C_d, N, RIGHT_ADD_VAL), {topTask});

    dagee::ATMItaskHandle bottomTask = gpuEx.launchTask(
        gpuEx.makeTask(blocks, threadsPerBlock, bottomK, A_d, B_d, C_d, N), {leftTask, rightTask});
    gpuEx.waitOnTask(bottomTask);
    //![Eager Launch]

  } else {
    std::cout << "Building Kite DAG\n";

    DagExec dagEx(gpuEx);
    DagExec::DAGptr dag = dagEx.makeDAG();

    DagExec::NodePtr topTask = dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, topK, A_d, N));
    DagExec::NodePtr leftTask =
        dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, B_d, N, LEFT_ADD_VAL));
    DagExec::NodePtr rightTask =
        dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, midK, A_d, C_d, N, RIGHT_ADD_VAL));
    DagExec::NodePtr bottomTask =
        dag->addNode(gpuEx.makeTask(blocks, threadsPerBlock, bottomK, A_d, B_d, C_d, N));

    dag->addFanOutEdges(topTask, {rightTask, leftTask});
    dag->addFanInEdges({rightTask, leftTask}, bottomTask);

    std::cout << "Executing Kite DAG\n";

    dagEx.execute(dag);
  }

  std::cout << "info: copy Device2Host\n";
  bufMgr.copyBufferToVec(A, A_d);

  checkOutput(A);
}

int main(int argc, char* argv[]) {
  kiteDag<true>();
  kiteDag<false>();
}
