#include "dagr/hsaCore.h"
#include "dagr/memory.h"

#include <cstdio>

#include <vector>
#include <iostream>

void printRegionInfo(const dagr::Region& region) noexcept {
  using Q = dagr::RegionQuery;

  std::printf("size = %zu, allocGranule = %zu, allocMaxSize = %zu, allocAlignment = %zu\n",
      Q::size(region), Q::allocGranule(region), Q::allocMaxSize(region), Q::allocAlignment(region));
}

template <typename V, typename StrT>
void printRegionVec(const V& regionVec, const StrT& name) noexcept {
  std::cout << "Printing info for " << name << std::endl;
  for (const auto& region: regionVec) {
    printRegionInfo(region);
  }
}

int main() {

  dagr::RuntimeState S;

  dagr::AgentRegionState regionState(S.gpuAgent(0));

  printRegionVec(regionState.mKernArgRegions, "mKernargRegions");
  printRegionVec(regionState.mCoarseRegions, "mCoarseRegions");
  printRegionVec(regionState.mFineRegions, "mFineRegions");


  return 0;
}
