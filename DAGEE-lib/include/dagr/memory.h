// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__MEMORY_H
#define DAGEE_INCLUDE_DAGR__MEMORY_H

#include "dagr/hsaCore.h"

#include "cpputils/Container.h"

#include <vector>

namespace dagr {



struct RegionQuery {

private:
  static size_t sizeTyAttr(const Region& region, const hsa_region_info_t& attrName) noexcept {
    size_t sz = 0ul;
    ASSERT_HSA(hsa_region_get_info(region, attrName, &sz));
    assert(sz != 0ul);
    return sz;
  }

  static bool hasGlobalFlags(const Region& region, const hsa_region_global_flag_t& flag) noexcept {

    assert(isGlobal(region));

    hsa_region_global_flag_t currFlags;
    ASSERT_HSA(hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &currFlags));

    return (currFlags & flag) != 0;
  }

public:

  static bool isGlobal(const Region& region) noexcept {
    hsa_region_segment_t segment;
    ASSERT_HSA(hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment));
    return (segment == HSA_REGION_SEGMENT_GLOBAL);
  }

  static bool isCoarseGrain(const Region& region) noexcept {
    return hasGlobalFlags(region, HSA_REGION_GLOBAL_FLAG_COARSE_GRAINED);
  }

  static bool isFineGrain(const Region& region) noexcept {
    return hasGlobalFlags(region, HSA_REGION_GLOBAL_FLAG_FINE_GRAINED);
  }

  static bool isKernArg(const Region& region) noexcept {
    return hasGlobalFlags(region, HSA_REGION_GLOBAL_FLAG_KERNARG);
  }

  static size_t size(const Region& region) noexcept {
    return sizeTyAttr(region, HSA_REGION_INFO_SIZE);
  }

  static size_t allocGranule(const Region& region) noexcept {
    return sizeTyAttr(region, HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE);
  }

  static size_t allocAlignment(const Region& region) noexcept {
    return sizeTyAttr(region, HSA_REGION_INFO_RUNTIME_ALLOC_ALIGNMENT);
  }

  static size_t allocMaxSize(const Region& region) noexcept {
    return sizeTyAttr(region, HSA_REGION_INFO_ALLOC_MAX_SIZE);
  }
};

struct AgentRegionState {

  using VecRegion = std::vector<Region>;

  Agent mAgent;

  VecRegion mKernArgRegions;
  VecRegion mCoarseRegions;
  VecRegion mFineRegions;

  void registerRegion(const Region& region) noexcept {
    if (!RegionQuery::isGlobal(region)) { 
      return;
    }

    if (RegionQuery::isKernArg(region)) {
      mKernArgRegions.emplace_back(region);

    } else if (RegionQuery::isCoarseGrain(region)) {
      mCoarseRegions.emplace_back(region);

    } else if (RegionQuery::isFineGrain(region)) {
      mFineRegions.emplace_back(region);

    } else {
      std::abort();
    }
  }

  hsa_status_t static visitRegion(Region region, void* data) noexcept {
    if(!data) {
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    AgentRegionState* outer = reinterpret_cast<AgentRegionState*>(data);
    assert(outer);
    outer->registerRegion(region);

    return HSA_STATUS_SUCCESS;
  }
  
  void init() noexcept {
    ASSERT_HSA(hsa_agent_iterate_regions(mAgent, visitRegion, this));
  }

  static const Region& getRegionImpl(const VecRegion& regVec, const size_t id) noexcept {
    assert(id < regVec.size());
    return regVec.at(id);
  }

public:

  explicit AgentRegionState(const Agent& a) noexcept: mAgent(a) {
    init();
  }

  const Region& kernArgRegion(const size_t id = 0ul) const noexcept {
    return getRegionImpl(mKernArgRegions, id);
  }
  
  const Region& coarseRegion(const size_t id = 0ul) const noexcept {
    return getRegionImpl(mCoarseRegions, id);
  }

  const Region& fineRegion(const size_t id = 0ul) const noexcept {
    return getRegionImpl(mFineRegions, id);
  }
};
using MemBlock = cpputils::MemBlock;

template <size_t ALLOC_SIZE_T=4096ul>
struct BackingHeapImpl {
  constexpr static const size_t ALLOC_SIZE = ALLOC_SIZE_T;

  using pointer = char*;

  Region mRegion;

  explicit BackingHeapImpl(const Region& r) noexcept : mRegion(r) {}

  MemBlock allocate() noexcept {

    char* ptr = nullptr;

    ASSERT_HSA(hsa_memory_allocate(mRegion, ALLOC_SIZE, reinterpret_cast<void**>(&ptr)));
    assert(ptr);

    return MemBlock {ptr, ptr + ALLOC_SIZE};
  }

  void deallocate(const MemBlock& blk) noexcept {
    ASSERT_HSA(hsa_memory_free(blk.mBeg));
  }
};

using BackingHeap = BackingHeapImpl<>;
using FixedSizeHeap = cpputils::FixedSizeHeapImpl<BackingHeap>;

struct KernArgHeap {

  using HeapMap = std::vector<FixedSizeHeap*>;

  // HSAIL spec says kernel arguments should align to 16 byte boundary
  constexpr static const size_t MIN_ALLOC_SIZE = 2 * sizeof(uintptr_t);
  constexpr static const size_t MAX_ALLOC_SIZE = BackingHeap::ALLOC_SIZE;
  constexpr static const size_t NUM_BUCKETS = MAX_ALLOC_SIZE / MIN_ALLOC_SIZE;

  BackingHeap mBackingHeap;
  HeapMap mHeapsForSz;

  explicit KernArgHeap(const Region& kreg) noexcept:
    mBackingHeap(kreg),
    mHeapsForSz(NUM_BUCKETS, nullptr)
  {
    assert(RegionQuery::isKernArg(kreg));
  }

  ~KernArgHeap() noexcept {
    for (auto* heap: mHeapsForSz) {
      delete heap;
    }
    mHeapsForSz.clear();
  }

  static void checkKernArgBlk(const MemBlock&  blk) {
    // assert(blk.begin() && !blk.empty());
    // check alignment to MIN_ALLOC_SIZE
    assert((reinterpret_cast<uint64_t>(blk.begin()) & (MIN_ALLOC_SIZE - 1)) == 0ul);
    // assert(blk.size() >= MIN_ALLOC_SIZE);
    assert(blk.size() % MIN_ALLOC_SIZE == 0);
  }

  FixedSizeHeap* heapForSize(size_t numBytesReq) noexcept {

    // round to next multiple of MIN_ALLOC_SIZE
    size_t roundedSz = (numBytesReq + (MIN_ALLOC_SIZE - 1)) & ~(MIN_ALLOC_SIZE - 1);
    size_t index = roundedSz / MIN_ALLOC_SIZE;
    assert(index < mHeapsForSz.size());

    auto*& heap = mHeapsForSz[index];
    if (!heap) {
      heap = new FixedSizeHeap(mBackingHeap, roundedSz);
    }
    return heap;
  }

  MemBlock allocate(size_t numBytesReq) noexcept {
    // Special case of kernel with no args
    if (numBytesReq == 0) {
      return MemBlock(nullptr, nullptr);
    }

    FixedSizeHeap* heap = heapForSize(numBytesReq);
    assert(heap);
    return heap->allocate();
  }


  void deallocate(const MemBlock& blk) noexcept {

    // Special case of kernel with no args
    if (blk.empty()) {
      assert(!blk.begin() && !blk.end());
      return;
    }

    assert(blk.size() >= MIN_ALLOC_SIZE);
    assert(blk.size() <= MAX_ALLOC_SIZE);

    FixedSizeHeap* heap = heapForSize(blk.size());
    assert(heap);
    heap->deallocate(blk);
  }

};


template <typename T>
void memCopy(T* dst, const T* src, size_t numElem) noexcept {
  ASSERT_HSA(hsa_memory_copy(dst, src, sizeof(T) * numElem));
}


struct DeviceMemManager {

  using GenericPtr = void*;
  using AllocSet = cpputils::VectorBasedSet<GenericPtr>;

  Region mCoarseRegion;
  AllocSet mAllocated;


  explicit DeviceMemManager(const Region& coarseReg) noexcept:
    mCoarseRegion(coarseReg),
    mAllocated()
  {
    assert(RegionQuery::isCoarseGrain(coarseReg));
  }

  ~DeviceMemManager() noexcept {
    for (GenericPtr p: mAllocated) {
      free(p);
    }
    mAllocated.clear();
  }

  template <typename T>
  T* allocate(size_t numElem) noexcept {
    assert(numElem > 0);

    T* ret = nullptr;
    ASSERT_HSA(hsa_memory_allocate(mCoarseRegion, sizeof(T) * numElem, &ret));
    assert(ret);

    mAllocated.insert(ret);

    return ret;
  }

  template <typename T>
  void free(T* ptr) noexcept {
    assert(ptr);
    assert(mAllocated.contains(ptr));
    hsa_memory_free(ptr);

    mAllocated.erase(ptr);
  }

  template <typename V, typename T = typename V::value_type>
  T* makeCopy(const V& vec) noexcept {

    T* devBuf = allocate<T>(vec.size());
    assert(devBuf);
    dagr::memCopy(devBuf, &vec[0], vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  void copyIn(T* devBuf, const V& vec) const noexcept {
    dagr::memCopy(devBuf, &vec[0], vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  void copyOut(V& vec, const T* devBuf) const noexcept {
    assert(devBuf);
    assert(vec.size() > 0);
    dagr::memCopy(&vec[0], devBuf, vec.size());
  }

};


} //end namespace dagr
#endif// DAGEE_INCLUDE_DAGR__MEMORY_H
