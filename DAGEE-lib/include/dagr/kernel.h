// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__KERNEL_H_
#define DAGEE_INCLUDE_DAGR__KERNEL_H_

#include "dagr/hsaCore.h"
#include "dagr/binary.h"
#include "dagr/memory.h"

#include "hip/hip_runtime.h"

#include <unordered_map>

namespace dagr {


namespace impl {
  template <typename U>
  using BareTy = typename std::remove_reference<typename std::remove_cv<U>::type>::type;

  template <size_t CURR_SZ, typename U, typename... Args>
  class ArgBufSizeImpl {
    using T = BareTy<U>;
    constexpr static const size_t ROUNDED = (CURR_SZ + alignof(T) - 1) & ~(alignof(T) - 1);
    constexpr static const size_t NEXT_SZ = ROUNDED + sizeof(T);

  public:
    constexpr static const size_t value = ArgBufSizeImpl<NEXT_SZ, Args...>::value;
  };

  template <size_t CURR_SZ> 
  struct ArgBufSizeImpl<CURR_SZ, void, void> {
    constexpr static const size_t value = CURR_SZ;
  };

  template <typename... Args>
  struct ArgBufSize {
    constexpr static const size_t value = ArgBufSizeImpl<0ul, Args...>::value;
  };

  template <> struct ArgBufSize<> {
    constexpr static const size_t value = 0ul;
  };

  template <typename BufT, typename U>
  void copyArgUpdateOffset(BufT& buffer, size_t& offset, U&& oneArg) noexcept {
    using T = BareTy<U>;
    // round up offset to alignof(T)
    // TODO: pick into a common utility function
    assert(offset < buffer.size());
    offset = (offset + alignof(T) - 1) & ~(alignof(T) - 1);
    assert(offset < buffer.size());

    // copy Arg
    new (buffer.begin() + offset) T(oneArg);

    // move to offset for next arg
    offset = offset + sizeof(T);
    assert(offset <= buffer.size());

  }

  template <typename BufT, typename... Args>
  void packKernArgs(BufT& buffer, Args&&... args) noexcept {
    size_t offset = 0ul;
    void((int [] ) { (0, copyArgUpdateOffset(buffer, offset, args))... });
  }

} // end naamespace impl


template <typename KI>
struct KernInstance {
  using MemBlock = cpputils::MemBlock;

  dim3 mBlocks;
  dim3 mThreadsPerBlock;
  const KI* mKernInfoPtr;
  MemBlock mKernArgBuf;


  template <typename... Args>
  KernInstance(const dim3& blks, const dim3& thrdsPerBlk, const KI* kinfo
      , const MemBlock& kernArgBuf, Args&&... args) noexcept:
    mBlocks(blks),
    mThreadsPerBlock(thrdsPerBlk),
    mKernInfoPtr(kinfo),
    mKernArgBuf(kernArgBuf) {

      impl::packKernArgs(mKernArgBuf, std::forward<Args>(args)...);
  }
};


struct GpuKernInfo {

  size_t mKernArgBufSz;
  FixedSizeHeap* mKernArgHeap = nullptr;
  // place at the end because  KernelObject is uint16_t
  KernelObject mKernObj;

  GpuKernInfo(const size_t kernArgBufSz, FixedSizeHeap* argHeap, const KernelObject& kobj) noexcept:
    mKernArgBufSz(kernArgBufSz),
    mKernArgHeap(argHeap),
    mKernObj(kobj)
  {
    assert(mKernArgHeap);
  }

  template <typename... Args>
  GpuKernInfo(const KernelObject& kobj, FixedSizeHeap* h) noexcept:
     mKernArgBufSz(impl::ArgBufSize<Args...>::value),
     mKernArgHeap(h),
     mKernObj(kobj)
  {
    assert(mKernArgHeap);
  }

  size_t kernArgBufSize() const noexcept {
    return mKernArgBufSz;
  }

  const KernelObject& kernObj() const noexcept {
    return mKernObj;
  }

  FixedSizeHeap* kernArgHeap() noexcept {
    return mKernArgHeap;
  }
};

using GpuKernInstance = KernInstance<GpuKernInfo>;


struct GpuKernInfoState {


  using KernelPtrLookup = dagee::KernelPtrToNameLookup<>;
  using GenericFuncPtr = dagee::GenericFuncPtr;

  using KernInfoByName = std::unordered_map<std::string, GpuKernInfo>;
  using KernInfoByPtr = std::unordered_map<GenericFuncPtr, const GpuKernInfo*>;

  ExecutableState& mBinaryState;
  KernArgHeap& mKernArgHeap;
  KernInfoByName mKernInfoByName;
  KernelPtrLookup mKernelPtrLookup;
  KernInfoByPtr mKernInfoByPtr;

  template <typename... Args>
  const GpuKernInfo* registerKernel(const char* kname) noexcept {

    auto it = mKernInfoByName.find(kname);

    if (it != mKernInfoByName.cend()) {
      return &(it->second);

    } else {

      KernelObject kobj = mBinaryState.kernelObjtectByName(kname);
      assert(kobj.isValid());

      if (!kobj.isValid()) { std::abort(); }

      constexpr auto argBufSz = impl::ArgBufSize<Args...>::value;
      FixedSizeHeap* argHeap = mKernArgHeap.heapForSize(argBufSz);
      assert(argHeap);

      auto iter = mKernInfoByName.insert(it, std::make_pair(kname, GpuKernInfo(argBufSz, argHeap, kobj)));

      return &(iter->second);
    }
  }

  template <typename... Args, typename FuncPtr = void (*)(Args...)>
  const GpuKernInfo* registerKernel(FuncPtr funcPtr) noexcept {
    auto fptr = reinterpret_cast<GenericFuncPtr>(funcPtr);

    auto it = mKernInfoByPtr.find(fptr);

    if (it != mKernInfoByPtr.cend()) {
      return it->second;

    } else {

      const auto& name = mKernelPtrLookup.name(fptr);
      // TODO(amber): FIXME: hip-clang related change where kernels have a .kd
      // suffix added to the name
      auto kname  = name + ".kd";

      const GpuKernInfo* kinfo = registerKernel<Args...>(kname.c_str());
      mKernInfoByPtr.insert(it, std::make_pair(fptr, kinfo));
      
      return kinfo;
    }

  }

  GpuKernInfoState(ExecutableState& binState, KernArgHeap& kargHeap) noexcept:
    mBinaryState(binState),
    mKernArgHeap(kargHeap),
    mKernInfoByName(),
    mKernelPtrLookup(),
    mKernInfoByPtr()
  {}
};

struct PacketFactory {

private:

  constexpr static const size_t BARRIER_PKT_NUM_PREDS = 5ul;

  template <typename PktT>
  static void zeroOut(PktT* pkt) noexcept {
    assert(pkt);
    *pkt = {0}; // init all members to 0
  }

  constexpr static const uint16_t KERNEL_DISPATCH_SETUP = 3u << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
  constexpr static const uint16_t BARRIER_SETUP = 0u << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;

  template <typename PktT, bool ATOMIC=true>
  static void updateHeaderAtomic(PktT* pkt, const PacketHeader& header) noexcept {
    assert(*reinterpret_cast<uint16_t*>(pkt) == 0 && "header field should be 0");

    // using 32 bits because 32 bit atomics are commonly supported
    uint32_t* pktHeader = reinterpret_cast<uint32_t*>(pkt);
    uint32_t updated = (*pktHeader) | uint32_t(header.value());

    if (ATOMIC) { 
      __atomic_store_n(pktHeader, updated, __ATOMIC_RELEASE);
    } else {
      *pktHeader = updated;
    }

  }

  template <typename PktT>
  static void updateHeaderSimple(PktT* pkt, const PacketHeader& header) noexcept {
    updateHeaderAtomic<PktT, false>(pkt, header);
    // updateHeaderAtomic<PktT, true>(pkt, header);
  }

public:

  static void init(KernelDispatchPkt* pkt, const PacketHeader& header, const dim3& numBlks, const dim3& threadsPerBlk, 
      size_t sharedMem, const KernelObject& kobj, const MemBlock& kernArgBuf, const Signal& sig) noexcept {

    zeroOut(pkt);

    pkt->setup = KERNEL_DISPATCH_SETUP;

    pkt->workgroup_size_x = threadsPerBlk.x;
    pkt->workgroup_size_y = threadsPerBlk.y;
    pkt->workgroup_size_z = threadsPerBlk.z;
    
    pkt->grid_size_x = numBlks.x * threadsPerBlk.x;
    pkt->grid_size_y = numBlks.y * threadsPerBlk.y;
    pkt->grid_size_z = numBlks.z * threadsPerBlk.z;

    pkt->group_segment_size = sharedMem;
    pkt->private_segment_size = 0ul; // TODO(amber): what is this for?

    assert(kobj.isValid());
    pkt->kernel_object = kobj.mVal;
    KernArgHeap::checkKernArgBlk(kernArgBuf);
    pkt->kernarg_address = kernArgBuf.begin();

    pkt->completion_signal = sig;

    updateHeaderSimple(pkt, header);
  }

  static void init(KernelDispatchPkt* pkt, const PacketHeader& header, const GpuKernInstance& ki, const Signal& compSig) noexcept {

    init(pkt, header, ki.mBlocks, ki.mThreadsPerBlock, 0ul,
        ki.mKernInfoPtr->kernObj(), ki.mKernArgBuf, compSig);
  }


  template <typename A>
  static void init(BarrierAndPkt* pkt, const PacketHeader& header, const Signal& compSig, const A& preds, const size_t numPreds) noexcept {

    zeroOut(pkt);
    pkt->completion_signal = compSig;

    assert(numPreds <= BARRIER_PKT_NUM_PREDS);

    if (numPreds > 0) {
      std::copy_n(&preds[0], std::min(numPreds, BARRIER_PKT_NUM_PREDS), &pkt->dep_signal[0]);
    }

    updateHeaderSimple(pkt, header);
  }


  /**
   * Given numPreds predecessors, how many barrier packets would be needed to 
   * reduce these to a single barrier packet if we use tree reduction where each
   * node is a barrier packet monitoring BARRIER_PKT_NUM_PREDS predecessors
   */
  static size_t barrierTreeSize(const size_t numPreds) noexcept {
    size_t ret = 0ul;

    for (size_t val = numPreds; val > 0; val /= BARRIER_PKT_NUM_PREDS) {
      // divide by BARRIER_PKT_NUM_PREDS rounding up
      ret += (val + BARRIER_PKT_NUM_PREDS - 1) / BARRIER_PKT_NUM_PREDS;
    }

    return ret;
  }

  //! assumes that pktArray has space for P = barrierTreeSize(numPreds) packets,
  //! and similarly sigArray has P free signals to use. 
  template <typename A>
  static void makeBarrierTree(BarrierAndPkt* pktArray, Signal* sigArray, const PacketHeader& header, const A& preds, const size_t numPreds) noexcept {

    // termination condition 
    if (numPreds < 2) {
      return;
    }

    // locations in pktArray and sigArray that have been used or consumed
    size_t usedIndex = 0ul;

    // generate packets for one level of the tree
    for (size_t i = 0; i < numPreds; i += BARRIER_PKT_NUM_PREDS) {
      size_t remSz = std::min(BARRIER_PKT_NUM_PREDS, i - numPreds);
      Signal compSig = sigArray[usedIndex];
      init(&pktArray[usedIndex], compSig, header, &preds[i], remSz);
      ++usedIndex;
    }

    // recurse to the next level
    makeBarrierTree(&pktArray[usedIndex], &sigArray[usedIndex], header, &sigArray[0], usedIndex);

  }
};
constexpr size_t PacketFactory::BARRIER_PKT_NUM_PREDS;


} // end namespace dagr


#endif// DAGEE_INCLUDE_DAGR__KERNEL_H_
