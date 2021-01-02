// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#include "kiteDagGpu.h"

#include "dagee/ATMIdagExecutor.h"
#include "hip/hip_runtime.h"

#include <cassert>
#include <cstdlib>

void topKernCpu(uint32_t* A_d, size_t N) {
  std::cout << "Starting Top Kernel" << std::endl;
  for (int i = 0; i < N; i++) {
    A_d[i] = INIT_VAL;
  }
}

void midKernCpu(uint32_t* A_d, uint32_t* B_d, size_t N, uint32_t addVal) {
  std::cout << "Starting Mid Kernel" << std::endl;
  for (int i = 0; i < N; i++) {
    B_d[i] = A_d[i] + addVal;
  }
}

void bottomKernCpu(uint32_t* A_d, uint32_t* B_d, uint32_t* C_d, size_t N) {
  std::cout << "Starting Bottom Kernel" << std::endl;
  for (int i = 0; i < N; i++) {
    A_d[i] = A_d[i] + B_d[i] + C_d[i];
  }
}

template <bool DYNAMIC_LAUNCH>
void test() {
  unsigned numThreads = 1;

  constexpr size_t N = 16;

  std::vector<uint32_t> A(N, 0);
  std::vector<uint32_t> B(N, 0);
  std::vector<uint32_t> C(N, 0);

  dagee::CpuExecutorAtmi cpuEx;

  auto A_h = A.data();
  auto B_h = B.data();
  auto C_h = C.data();

  auto topK = cpuEx.registerKernel<uint32_t*, size_t>(&topKernCpu);
  auto midK = cpuEx.registerKernel<uint32_t*, uint32_t*, size_t, uint32_t>(&midKernCpu);
  auto bottomK = cpuEx.registerKernel<uint32_t*, uint32_t*, uint32_t*, size_t>(&bottomKernCpu);

  if (DYNAMIC_LAUNCH) {
    std::cout << "Running CPU kernels synchronously one at a time\n";
    auto topTask = cpuEx.launchTask(cpuEx.makeTask(numThreads, topK, A_h, N));

    auto leftTask =
        cpuEx.launchTask(cpuEx.makeTask(numThreads, midK, A_h, B_h, N, LEFT_ADD_VAL), {topTask});
    auto rightTask =
        cpuEx.launchTask(cpuEx.makeTask(numThreads, midK, A_h, C_h, N, RIGHT_ADD_VAL), {topTask});

    auto bottomTask = cpuEx.launchTask(cpuEx.makeTask(numThreads, bottomK, A_h, B_h, C_h, N),
                                       {leftTask, rightTask});
    cpuEx.waitOnTask(bottomTask);

  } else {
    std::cout << "Building Kite DAG\n";
    dagee::ATMIdagExecutor<dagee::CpuExecutorAtmi> dagEx(cpuEx);
    auto* dag = dagEx.makeDAG();

    auto topTask = dag->addNode(cpuEx.makeTask(numThreads, topK, A_h, N));
    auto leftTask = dag->addNode(cpuEx.makeTask(numThreads, midK, A_h, B_h, N, LEFT_ADD_VAL));
    auto rightTask = dag->addNode(cpuEx.makeTask(numThreads, midK, A_h, C_h, N, RIGHT_ADD_VAL));
    auto bottomTask = dag->addNode(cpuEx.makeTask(numThreads, bottomK, A_h, B_h, C_h, N));

    dag->addFanOutEdges(topTask, {leftTask, rightTask});
    dag->addFanInEdges({leftTask, rightTask}, bottomTask);

    std::cout << "Executing Kite DAG\n";

    dagEx.execute(dag);
  }

  checkOutput(A);
}

int main(int argc, char* argv[]) {
  test<true>();
  test<false>();
}
