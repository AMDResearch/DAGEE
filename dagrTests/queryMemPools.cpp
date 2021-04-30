#include "dagr/hsaCore.h"
#include "dagr/memory.h"

#include <cstdio>

#include <vector>
#include <iostream>

void printPoolInfo(const dagr::MemPool& pool) noexcept {
  using Q = dagr::MemPoolQuery;

  std::printf("size = %zu, allocGranule = %zu, allocMaxSize = %zu, allocAlignment = %zu\n",
      Q::size(pool), Q::allocGranule(pool), Q::allocMaxSize(pool), Q::allocAlignment(pool));
}

template <typename V, typename StrT>
void printPoolVec(const V& poolVec, const StrT& name) noexcept {
  std::cout << "Printing info for " << name << std::endl;
  for (const auto& pool: poolVec) {
    printPoolInfo(pool);
  }
}

int main() {

  dagr::RuntimeState S;

  dagr::AgentMemPoolState poolState(S.mGpuAgents.mAgentVec.front());

  printPoolVec(poolState.mKernargPools, "mKernargPools");
  printPoolVec(poolState.mCoarsegrainPools, "mCoarsegrainPools");
  printPoolVec(poolState.mFinegrainPools, "mFinegrainPools");


  return 0;
}
