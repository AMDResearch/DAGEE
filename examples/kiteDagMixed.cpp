// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#include "dagee/ATMIalloc.h"
#include "dagee/ATMIcpuExecutor.h"
#include "dagee/ATMIgpuExecutor.h"
#include "dagee/ATMImixedDAGexecutor.h"

#include "hip/hip_runtime.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

constexpr int INIT_VAL = 1;
constexpr int MULTIPLIER = 4;
constexpr int EXPECTED = (INIT_VAL + 1) * MULTIPLIER;
constexpr unsigned BLOCKS = 1;
constexpr unsigned THREADS_PER_BLOCK = 16;
constexpr unsigned NUM_CPU_THREADS = BLOCKS;

constexpr size_t N = BLOCKS * THREADS_PER_BLOCK;

void topKernCpu(uint32_t* A_d, size_t N) {
  // std::cout << "Running Top Kern" << std::endl;
  for (int i = 0; i < N; i++) {
    A_d[i] = INIT_VAL;
  }
}

__global__ void midKernGpu(uint32_t* A_d, size_t N) {
  size_t i = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  if (i < N) {
    A_d[i]++;
  }
}

void bottomKernCpu(uint32_t* A_d, size_t N) {
  // std::cout << "Running Bottom Kern" << std::endl;
  for (int i = 0; i < N; i++) {
    A_d[i] *= MULTIPLIER;
  }
}

template <typename V>
void checkOutput(const V& A) {
  size_t index = 0;
  bool correct = true;
  for (const auto& e : A) {
    if (e != EXPECTED) {
      correct = false;
      std::fprintf(stderr, "Wrong value %d at index %zd. Expected %d\n", e, index, EXPECTED);
    }
    ++index;
  }

  if (correct) {
    std::cout << "OK. Output check passed" << std::endl;
  } else {
    std::cerr << "ERROR. Output check failed" << std::endl;
    std::abort();
  }
}

template <bool DYNAMIC_LAUNCH>
void sharedMemKiteDag() {
  std::vector<uint32_t> A(N, 0);

  dagee::CpuExecutorAtmi cpuEx;
  dagee::GpuExecutorAtmi gpuEx;

  dagee::AllocManagerAtmi bufMgr;
  auto* A_h = bufMgr.makeSharedCopy(A);

  auto topCpuK = cpuEx.registerKernel<uint32_t*, size_t>(&topKernCpu);
  auto midGpuK = gpuEx.registerKernel<uint32_t*, size_t>(&midKernGpu);
  auto bottomCpuK = cpuEx.registerKernel<uint32_t*, size_t>(&bottomKernCpu);

  if (DYNAMIC_LAUNCH) {
    std::cout << "\nLaunching one kernel at a time\n";
    auto topTask = cpuEx.launchTask(cpuEx.makeTask(NUM_CPU_THREADS, topCpuK, A_h, N));

    auto leftTask =
        gpuEx.launchTask(gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, A_h, N / 2), {topTask});

    auto rightTask = gpuEx.launchTask(
        gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, &A_h[N / 2], N), {topTask});

    auto bottomTask = cpuEx.launchTask(cpuEx.makeTask(NUM_CPU_THREADS, bottomCpuK, A_h, N),
                                       {leftTask, rightTask});
    cpuEx.waitOnTask(bottomTask);

  } else {
    std::cout << "\nBuilding mixed Kite DAG\n";

    auto dagEx = dagee::makeMixedDagExecutor(cpuEx, gpuEx);
    auto* dag = dagEx.makeDAG();

    auto topTask = dag->addNode(cpuEx.makeTask(NUM_CPU_THREADS, topCpuK, A_h, N));
    auto leftTask = dag->addNode(gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, A_h, N / 2));
    auto rightTask =
        dag->addNode(gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, &A_h[N / 2], N));
    auto bottomTask = dag->addNode(cpuEx.makeTask(NUM_CPU_THREADS, bottomCpuK, A_h, N));

    dag->addFanOutEdges(topTask, {leftTask, rightTask});
    dag->addFanInEdges({leftTask, rightTask}, bottomTask);

    std::cout << "Executing Kite DAG\n";
    dagEx.execute(dag);
  }
  bufMgr.copyBufferToVec(A, A_h);
  checkOutput(A);
}

template <bool DYNAMIC_LAUNCH>
void memcpyKiteDag() {
  std::vector<uint32_t> A(N, 0);

  dagee::CpuExecutorAtmi cpuEx;
  dagee::GpuExecutorAtmi gpuEx;
  dagee::MemCopyExecutorAtmi memEx;

  dagee::AllocManagerAtmi bufMgr;

  auto* A_h = A.data();
  auto* A_d = bufMgr.makeDeviceCopy(A);

  auto topCpuK = cpuEx.registerKernel<uint32_t*, size_t>(&topKernCpu);
  auto midGpuK = gpuEx.registerKernel<uint32_t*, size_t>(&midKernGpu);
  auto bottomCpuK = cpuEx.registerKernel<uint32_t*, size_t>(&bottomKernCpu);

  if (DYNAMIC_LAUNCH) {
    std::cout << "\nLaunching one kernel at a time\n";
    auto topTask = cpuEx.launchTask(cpuEx.makeTask(NUM_CPU_THREADS, topCpuK, A_h, N));

    auto h2dCopyTask = memEx.launchTask(memEx.makeTask(A_h, A_d, N), {topTask});

    auto leftTask = gpuEx.launchTask(gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, A_d, N / 2),
                                     {h2dCopyTask});

    auto rightTask = gpuEx.launchTask(
        gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, &A_d[N / 2], N), {h2dCopyTask});

    auto d2hCopyTask = memEx.launchTask(memEx.makeTask(A_d, A_h, N), {leftTask, rightTask});

    auto bottomTask =
        cpuEx.launchTask(cpuEx.makeTask(NUM_CPU_THREADS, bottomCpuK, A_h, N), {d2hCopyTask});
    cpuEx.waitOnTask(bottomTask);

  } else {
    std::cout << "\nBuilding mixed Kite DAG\n";
    auto dagEx = dagee::makeMixedDagExecutor(cpuEx, gpuEx, memEx);

    auto* dag = dagEx.makeDAG();

    auto topTask = dag->addNode(cpuEx.makeTask(NUM_CPU_THREADS, topCpuK, A_h, N));
    auto h2dCopyTask = dag->addNode(memEx.makeTask(A_h, A_d, N));
    auto leftTask = dag->addNode(gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, A_d, N / 2));
    auto rightTask =
        dag->addNode(gpuEx.makeTask(BLOCKS, THREADS_PER_BLOCK, midGpuK, &A_d[N / 2], N));
    auto d2hCopyTask = dag->addNode(memEx.makeTask(A_d, A_h, N));
    auto bottomTask = dag->addNode(cpuEx.makeTask(NUM_CPU_THREADS, bottomCpuK, A_h, N));

    dag->addEdge(topTask, h2dCopyTask);
    dag->addFanOutEdges(h2dCopyTask, {rightTask, leftTask});
    dag->addFanInEdges({rightTask, leftTask}, d2hCopyTask);
    dag->addEdge(d2hCopyTask, bottomTask);
    std::cout << "Executing Kite DAG\n";
    dagEx.execute(dag);
  }
  checkOutput(A);
}

int main(int argc, char* argv[]) {
  std::cout << "Shared Memory Kite DAG" << std::endl;
  std::cout << "======================" << std::endl;
  sharedMemKiteDag<true>();
  sharedMemKiteDag<false>();

  std::cout << "\nMemcpy Kite DAG" << std::endl;
  std::cout << "===============" << std::endl;
  memcpyKiteDag<true>();
  memcpyKiteDag<false>();
}
