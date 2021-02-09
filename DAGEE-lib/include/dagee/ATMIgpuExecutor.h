// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ATMI_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_ATMI_EXECUTOR_H

#include "dagee/ATMIbaseExecutor.h"
#include "dagee/ATMIcoreDef.h"
#include "dagee/ProgInfo.h"

namespace dagee {

using ATMIgpuKernelInfo = atmi_kernel_t;

// TODO(amber): Store mKernArgOffsets as part of ATMIgpuKernelInfo
template <typename AllocFactory>
struct ATMIgpuKernelInstance {
  using VecArgAddr = typename AllocFactory::template Vec<void*>;

  dim3 mBlocks;
  dim3 mThreadsPerBlock;
  ATMIgpuKernelInfo mKernInfo;
  typename AllocFactory::template Vec<size_t> mKernArgOffsets;
  typename AllocFactory::template Vec<std::uint8_t> mKernArgs;
  ATMItaskHandle mATMItaskHandle;

  template <typename... Args>
  ATMIgpuKernelInstance(const dim3& blocks, const dim3& threadsPerBlock,
                        const ATMIgpuKernelInfo& kinfo, Args&&... args)
      : mBlocks(blocks),
        mThreadsPerBlock(threadsPerBlock),
        mKernInfo(kinfo),
        mKernArgOffsets(),
        mKernArgs(),
        mATMItaskHandle() {
    constexpr auto NUM_ARGS = sizeof...(Args);
    mKernArgOffsets.reserve(NUM_ARGS);
    mKernArgs = impl::packKernArgsCompOffset(mKernArgOffsets, std::forward<Args>(args)...);
  }

  VecArgAddr argAddresses(void) const {
    /*
     * XXX: Must recompute addresses everytime afresh because *this object may
     * have been copied leading to vectors' internal storage being relocated
     */
    VecArgAddr argAddrs;
    argAddrs.reserve(mKernArgOffsets.size());

    argAddrs.clear();

    using V = typename std::remove_cv<
        typename std::remove_reference<decltype(mKernArgs)>::type::value_type>::type;

    for (const auto& off : mKernArgOffsets) {
      argAddrs.emplace_back(const_cast<V*>(mKernArgs.data() + off));
    }

    return argAddrs;
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<> >
struct GpuKernelRegAtmiPolicy {
  using KernelInfo = ATMIgpuKernelInfo;

 protected:
  using KernPtrLookup = KernelPtrToNameLookup<AllocFactory>;

  using KernelInfoMap = typename AllocFactory::template HashMap<GenericFuncPtr, KernelInfo>;

  KernPtrLookup mKernPtrLookup;

  KernelInfoMap mGpuKernels;

  void init(void) {
    KernelSectionParser<AllocFactory> kparser;
    kparser.parseKernelSections();

    std::vector<void*> blobPtrs;
    std::vector<size_t> blobSizes;
    std::vector<atmi_platform_type_t> blobPlats;
    size_t numBlobs = 0;

    for (const auto& blob : kparser.codeBlobs()) {
      if (!blob.isEmpty() && blob.isGPU()) {
        blobPtrs.emplace_back(const_cast<char*>(blob.data()));
        blobSizes.emplace_back(blob.size());
        blobPlats.emplace_back(AMDGCN);
        ++numBlobs;
      }
    }

    if (numBlobs == 0) {
      std::cerr << "No kernel code in the executable" << std::endl;
      std::abort();
    }

    CHECK_ATMI(atmi_module_register_from_memory(blobPtrs.data(), blobSizes.data(), blobPlats.data(),
                                                numBlobs));
  }

  void finish(void) {
    for (const auto& k : mGpuKernels) {
      CHECK_ATMI(atmi_kernel_release(k.second));
    }
    mGpuKernels.clear();
    // hsa_shut_down(); // Causes error when HIP/HCC (Kalmar) runtime tries
    // hsa_shut_down
  }

 public:
  GpuKernelRegAtmiPolicy() { init(); }
  ~GpuKernelRegAtmiPolicy() { finish(); }

  // FIXME: arguments must be provided by copy
  template <typename... Args, typename FuncPtr = void (*)(Args...)>
  KernelInfo registerKernel(FuncPtr funcPtr) {
    auto fptr = reinterpret_cast<GenericFuncPtr>(funcPtr);

    auto kiter = mGpuKernels.find(fptr);

    if (kiter != mGpuKernels.cend()) {
      return kiter->second;

    } else {
      const auto& name = mKernPtrLookup.name(fptr);

      atmi_kernel_t kernel;
      constexpr const unsigned numArgs = sizeof...(Args);
      size_t argSizes[] = {sizeof(Args)...};
      // std::printf("kernel name lookup = %s\n", name.c_str());
      CHECK_ATMI(atmi_kernel_create(&kernel, numArgs, argSizes, 1, ATMI_DEVTYPE_GPU, name.c_str()));

      mGpuKernels.insert(kiter, std::make_pair(fptr, kernel));

      return kernel;
    }
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<>, const int SYNCHRONOUS = ATMI_FALSE>
struct GpuKernelLaunchAtmiPolicy {
  using TaskInstance = ATMIgpuKernelInstance<AllocFactory>;
  using KernelInfo = ATMIgpuKernelInfo;

  using Sync = GpuKernelLaunchAtmiPolicy<AllocFactory, ATMI_TRUE>;
  using Async = GpuKernelLaunchAtmiPolicy<AllocFactory, ATMI_FALSE>;

  static ATMIlaunchParam initLaunchParam(const TaskInstance& ti) {
    // TODO: ATMIlaunchParam needs a proper constructor
    auto lp = impl::initLaunchParam();

    lp.groupDim[0] = ti.mThreadsPerBlock.x;
    lp.groupDim[1] = ti.mThreadsPerBlock.y;
    lp.groupDim[2] = ti.mThreadsPerBlock.z;

    lp.gridDim[0] = ti.mThreadsPerBlock.x * ti.mBlocks.x;
    lp.gridDim[1] = ti.mThreadsPerBlock.y * ti.mBlocks.y;
    lp.gridDim[2] = ti.mThreadsPerBlock.z * ti.mBlocks.z;

    lp.synchronous = SYNCHRONOUS;
    lp.place = ATMI_PLACE_GPU(0, 0);

    return lp;
  }

  static ATMIkernelHandle kernelHandle(const TaskInstance& ti) { return ti.mKernInfo; }

  template <typename... Args>
  static TaskInstance makeTask(const dim3& blocks, const dim3& threadsPerBlock,
                               const KernelInfo& kinfo, Args&&... args) {
    return TaskInstance(blocks, threadsPerBlock, kinfo, std::forward<Args>(args)...);
  }
};

using GpuExecutorAtmi =
    ExecutorSkeletonAtmi<GpuKernelRegAtmiPolicy<>,
                         impl::KernelLaunchAtmiPolicy<GpuKernelLaunchAtmiPolicy<>::Async> >;
using SynchronousGpuExecutorAtmi =
    ExecutorSkeletonAtmi<GpuKernelRegAtmiPolicy<>,
                         impl::KernelLaunchAtmiPolicy<GpuKernelLaunchAtmiPolicy<>::Sync> >;

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_ATMI_EXECUTOR_H
