// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ATMI_CPU_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_ATMI_CPU_EXECUTOR_H

#include "dagee/ATMIbaseExecutor.h"
#include "dagee/ATMIcoreDef.h"

namespace dagee {

struct ATMIcpuKernelInfo {
  atmi_kernel_t mKern;
  GenericFuncPtr mFuncPtr;
};

template <typename AllocFactory>
struct ATMIcpuKernelInstance {
  using VecArgAddr = typename AllocFactory::template Vec<void*>;

  dim3 mThreads;
  ATMIcpuKernelInfo mCpuKernelInfo;
  typename AllocFactory::template Vec<size_t> mKernArgOffsets;
  typename AllocFactory::template Vec<std::uint8_t> mKernArgs;
  ATMItaskHandle mATMItaskHandle;

  template <typename... Args>
  ATMIcpuKernelInstance(const dim3& numThreads, const ATMIcpuKernelInfo& cpuKinfo, Args&&... args)
      : mThreads(numThreads),
        mCpuKernelInfo(cpuKinfo),
        mKernArgOffsets(),
        mKernArgs(),
        mATMItaskHandle() {
    constexpr auto NUM_ARGS = sizeof...(Args);
    mKernArgOffsets.reserve(NUM_ARGS);
    mKernArgs = impl::packKernArgsCompOffset(mKernArgOffsets, std::forward<Args>(args)...);
  }

  VecArgAddr argAddresses(void) const {
    VecArgAddr argAddrs;
    argAddrs.reserve(mKernArgOffsets.size());

    argAddrs.clear();

    using V = typename std::remove_cv<
        typename std::remove_reference<decltype(mKernArgs)>::type::value_type>::type;

    // auto fptr = mCpuKernelInfo.mFuncPtr;
    argAddrs.emplace_back(
        reinterpret_cast<void*>(&const_cast<ATMIcpuKernelInfo&>(mCpuKernelInfo).mFuncPtr));
    for (const auto& off : mKernArgOffsets) {
      argAddrs.emplace_back(const_cast<V*>(mKernArgs.data() + off));
    }

    return argAddrs;
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<> >
struct CpuKernelRegAtmiPolicy {
 protected:
  // using CpuKernelInfoMap = std::unordered_map<GenericFuncPtr, atmi_kernel_t>;
  using CpuKernelInfoMap = typename AllocFactory::template HashMap<GenericFuncPtr, atmi_kernel_t>;

  CpuKernelInfoMap mCpuKernels;

  void finish(void) {
    for (const auto& k : mCpuKernels) {
      CHECK_ATMI(atmi_kernel_release(k.second));
    }
    mCpuKernels.clear();
  }

  template <typename F, typename... Args>
  static void wrappedFunc(typename std::add_pointer<F>::type fptr,
                          typename std::add_pointer<Args>::type... argsPtrs) {
    (*fptr)(*argsPtrs...);
  }

 public:
  using KernelInfo = ATMIcpuKernelInfo;

  ~CpuKernelRegAtmiPolicy(void) { finish(); }

  template <typename... Args, typename FuncPtr = void (*)(Args...)>
  KernelInfo registerKernel(FuncPtr funcPtr) {
    auto wf = &wrappedFunc<FuncPtr, Args...>;
    auto wfKey = reinterpret_cast<GenericFuncPtr>(wf);
    auto fptr = reinterpret_cast<GenericFuncPtr>(funcPtr);

    auto kiter = mCpuKernels.find(wfKey);

    if (kiter != mCpuKernels.cend()) {
      return KernelInfo{kiter->second, fptr};
    } else {
      atmi_kernel_t kinfo; // Rename kinfo to something else
      constexpr const unsigned numArgs = sizeof...(Args) + 1;
      // NOTE(Amber): while wrappedFunc takes args by pointer, we still
      // register the sizes of original Args & FuncPtr
      // size_t argSizes[] = { sizeof(typename std::add_pointer<FuncPtr>::type),
      // sizeof(typename std::add_pointer<Args>::type)... };
      static_assert(sizeof(FuncPtr) == sizeof(GenericFuncPtr),
                    "bad cast from FuncPtr to GenericFuncPtr");
      size_t argSizes[] = {sizeof(GenericFuncPtr), sizeof(Args)...};

      CHECK_ATMI(atmi_kernel_create(&kinfo, numArgs, argSizes, 1, ATMI_DEVTYPE_CPU,
                                    reinterpret_cast<atmi_generic_fp>(wf)));

      mCpuKernels.insert(kiter, std::make_pair(wfKey, kinfo));

      return KernelInfo{kinfo, fptr};
    }
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<>, const int SYNCHRONOUS = ATMI_FALSE>
struct CpuKernelLaunchAtmiPolicy {
  using TaskInstance = ATMIcpuKernelInstance<AllocFactory>;
  using KernelInfo = ATMIcpuKernelInfo;

  using Async = CpuKernelLaunchAtmiPolicy<AllocFactory, ATMI_FALSE>;
  using Sync = CpuKernelLaunchAtmiPolicy<AllocFactory, ATMI_TRUE>;

  static ATMIlaunchParam initLaunchParam(const TaskInstance& ti) {
    auto lp = impl::initLaunchParam();

    lp.groupDim[0] = 1;
    lp.groupDim[1] = 1;
    lp.groupDim[2] = 1;

    lp.gridDim[0] = ti.mThreads.x;
    lp.gridDim[1] = ti.mThreads.y;
    lp.gridDim[2] = ti.mThreads.z;

    lp.synchronous = SYNCHRONOUS;
    lp.place = ATMI_PLACE_CPU(0, 0);

    return lp;
  }

  static ATMIkernelHandle kernelHandle(const TaskInstance& ti) { return ti.mCpuKernelInfo.mKern; }

  template <typename... Args>
  static TaskInstance makeTask(const dim3& threads, const KernelInfo& kinfo, Args&&... args) {
    return TaskInstance(threads, kinfo, std::forward<Args>(args)...);
  }
};

using CpuExecutorAtmi =
    ExecutorSkeletonAtmi<CpuKernelRegAtmiPolicy<>,
                         impl::KernelLaunchAtmiPolicy<CpuKernelLaunchAtmiPolicy<>::Async> >;
using SynchronousCpuExecutorAtmi =
    ExecutorSkeletonAtmi<CpuKernelRegAtmiPolicy<>,
                         impl::KernelLaunchAtmiPolicy<CpuKernelLaunchAtmiPolicy<>::Sync> >;
} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_ATMI_CPU_EXECUTOR_H
