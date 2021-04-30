// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.
#ifndef INCLUDE_CPPUTILS_HEAP_H_
#define INCLUDE_CPPUTILS_HEAP_H_

#include <cassert>
#include <cstdio>
#include <cstddef>

#include <vector>

namespace cpputils {

template <typename PtrT=char*>
struct MemBlockImpl {

  using pointer = PtrT;
  PtrT mBeg = nullptr;
  PtrT mEnd = nullptr;

  MemBlockImpl(const PtrT& b, const PtrT& e) : mBeg(b), mEnd(e) {}

  const PtrT& begin(void) const noexcept { return mBeg; }
  const PtrT& end(void) const noexcept { return mEnd; }

  size_t size(void) const noexcept {
    assert(mEnd >= mBeg);
    return static_cast<size_t>(mEnd - mBeg);
  }

  ptrdiff_t distance(void) const noexcept { return (mEnd - mBeg); }

  bool empty(void) const noexcept { return mBeg == mEnd; }
};

using MemBlock =  MemBlockImpl<>;

/**
 * FixedSizeHeap allocates blocks of a fixed size (mAllocSz). It requests a larger block
 * from the BackingHeapT and chops it up into smaller blocks of mAllocSz
 */
template <typename BackingHeapT>
class FixedSizeHeapImpl {
  // FIXME(amber): Fixed size heap currently requests huge pages from the
  // mBackingHeap but never frees them back until it's destroyed. Freed chunks go in the mFreeBlocks below.
  // If we track what chunks belong to which page of BackingHeapT, we can free a
  // huge page when all its chunks have been freed by FixedSizeHeap

  using BlockList = std::vector<MemBlock>;
  using pointer = char*;

  size_t mAllocSz;
  BackingHeapT& mBackingHeap;
  BlockList mFreeBlocks;
  BlockList mBigBlocks;

  void replenish(void) {
    assert(mFreeBlocks.empty());

    MemBlock bigBlk = mBackingHeap.allocate();
    assert(bigBlk.size() >= mAllocSz && "must allocate at least 1 block of mAllocSz");

    if (bigBlk.empty()) {
      std::fprintf(stderr, "FixedSizeHeapImpl::replenish() :  mBackingHeap.allocate() failed\n");
      std::abort();
    }

    mBigBlocks.emplace_back(bigBlk);

    auto* beg = bigBlk.begin();
    auto* end = bigBlk.end();
    for (auto* i = beg; (i+mAllocSz) <= end; i += mAllocSz) {
      mFreeBlocks.emplace_back(i, i + mAllocSz);
    }
  }

 public:
  FixedSizeHeapImpl(BackingHeapT& backHeap, const size_t allocSz)
      : mAllocSz(allocSz), mBackingHeap(backHeap) {
    // TODO(amber) FIXME other fields are default constructed?
    // TODO(amber) FIXME: change assert because ALLOCATION_SIZE is undefined?
    // assert(BackingHeapT::ALLOCATION_SIZE % mAllocSz == 0 &&
    //       "allocation size must be a factor of CoreHeap::ALLOCATION_SIZE");
  }

  ~FixedSizeHeapImpl(void) noexcept {
    for (const auto& bigBlk : mBigBlocks) {
      mBackingHeap.deallocate(bigBlk);
    }
    mBigBlocks.clear();
    mFreeBlocks.clear();
  }

  size_t allocationSize(void) const noexcept { return mAllocSz; }

  /**
   * allocate without alignment constraints
   */
  MemBlock allocate() noexcept {
    if (mFreeBlocks.empty()) {
      replenish();
    }

    if (mFreeBlocks.empty()) {
      assert(false && "allocation failed due to out of memory");
      return MemBlock{nullptr, nullptr};
    }

    MemBlock ret = mFreeBlocks.back();
    mFreeBlocks.pop_back();
    assert(ret.size() == mAllocSz && "allocation size mismatch. Bad blk");
    return ret;
  }

  void deallocate(const MemBlock& blk) {
    assert(blk.size() == mAllocSz && "attempting to free a bad blk");
    mFreeBlocks.push_back(blk);
  }
};




}// end namespace cpputils

#endif// INCLUDE_CPPUTILS_HEAP_H_
