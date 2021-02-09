// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ATMI_CORE_DEF_H
#define DAGEE_INCLUDE_DAGEE_ATMI_CORE_DEF_H

#include "dagee/AllocFactory.h"
#include "dagee/CheckStatus.h"
#include "dagee/coreDef.h"

#include "atmi.h"
#include "atmi_runtime.h"

#include "hip/hip_runtime.h"

#include <cassert>

#include <type_traits>

namespace dagee {

using ATMIkernelHandle = atmi_kernel_t;
using ATMItaskHandle = atmi_task_handle_t;
using ATMIlaunchParam = atmi_lparm_t;
using ATMIcopyParam = atmi_cparm_t;

enum MemType : std::underlying_type<atmi_devtype_t>::type {
  // NOTE (Kiran) : Allocations with SHARED MemType is accessible from both CPU
  // and GPU.
  SHARED = ATMI_DEVTYPE_CPU,
  GPU = ATMI_DEVTYPE_GPU,
  // NOTE (Kiran) : Below enums could be made available as and when their
  // purpose is realized.
  // ALL = ATMI_DEVTYPE_ALL,
  // DGPU = ATMI_DEVTYPE_dGPU,
  // IGPU = ATMI_DEVTYPE_iGPU,
  // DSP = ATMI_DEVTYPE_DSP,
};

class InitAtmiBase {
  void init() { CHECK_ATMI(atmi_init(ATMI_DEVTYPE_ALL)); }
  void finish() { /*atmi_finalize();*/
  } // TODO(amber/ashwin) FIXME: this causes a crash when invoked multiple times by different
    // derived instances

 public:
  InitAtmiBase() { init(); }
  ~InitAtmiBase() { finish(); }
};

namespace impl {

template <typename I>
void waitOnTasks(const I& taskHbeg, const I& taskHend) {
  for (auto i = taskHbeg; i != taskHend; ++i) {
    atmi_task_wait(*i);
  }
}

void waitOnTasks(std::initializer_list<ATMItaskHandle> l) { waitOnTasks(l.begin(), l.end()); }

void waitOnTask(const ATMItaskHandle& t) { atmi_task_wait(t); }

template <typename I>
void activateTasks(const I& taskHbeg, const I& taskHend) {
  for (auto i = taskHbeg; i != taskHend; ++i) {
    atmi_task_activate(*i);
  }
}

void activateTasks(std::initializer_list<ATMItaskHandle> l) { activateTasks(l.begin(), l.end()); }

void activateTask(const ATMItaskHandle& t) { atmi_task_activate(t); }

inline atmi_mem_place_t initMemPlaceAtmi(MemType memType) {
  atmi_mem_place_t place;
  place.node_id = 0;
  place.dev_type = static_cast<atmi_devtype_t>(memType);
  place.dev_id = 0;
  place.mem_id = 0;
  return place;
}

template <typename T>
T initLparamCparam() {
  T param;

  param.group = ATMI_DEFAULT_TASKGROUP_HANDLE;
  param.groupable = ATMI_FALSE;
  param.profilable = ATMI_FALSE;
  param.synchronous = ATMI_FALSE;
  param.num_required = 0;
  param.requires = nullptr;
  param.num_required_groups = 0;
  param.required_groups = nullptr;
  param.task_info = nullptr;

  return param;
}

ATMIcopyParam initCopyParam() { return initLparamCparam<ATMIcopyParam>(); }

ATMIlaunchParam initLaunchParam() {
  ATMIlaunchParam lp = initLparamCparam<ATMIlaunchParam>();
  lp.gridDim[0] = 1;
  lp.gridDim[1] = 1;
  lp.gridDim[2] = 1;
  lp.groupDim[0] = 1;
  lp.groupDim[1] = 1;
  lp.groupDim[2] = 1;
  // TODO(amber) FIXME: use ATMI_FENCE_SCOPE_DEVICE for internal nodes in a DAG
  // while ATMI_FENCE_SCOPE_SYSTEM for sink nodes
  lp.acquire_scope = ATMI_FENCE_SCOPE_SYSTEM;
  lp.release_scope = ATMI_FENCE_SCOPE_SYSTEM;

  lp.atmi_id = ATMI_VRM;
  lp.kernel_id = 0;
  lp.task_info = nullptr;
  lp.continuation_task = ATMI_NULL_TASK_HANDLE;
  lp.place = ATMI_PLACE_ANY(0);

  return lp;
}

template <typename TargetLaunchPolicy, typename AllocFactory = dagee::StdAllocatorFactory<> >
struct KernelLaunchAtmiPolicy {
  using TaskInstance = typename TargetLaunchPolicy::TaskInstance;

 protected:
  static ATMItaskHandle makeInternalTask(const TaskInstance& ti, const ATMItaskHandle* predsArr,
                                         size_t numPreds) {
    auto lp = TargetLaunchPolicy::initLaunchParam(ti);
    // TODO: find a way to remove this cast
    lp.requires = const_cast<ATMItaskHandle*>(predsArr);
    lp.num_required = numPreds;

    return atmi_task_create(&lp, TargetLaunchPolicy::kernelHandle(ti), ti.argAddresses().data());
  }

  static ATMItaskHandle launchInternalTask(const TaskInstance& ti, const ATMItaskHandle* predsArr,
                                           size_t numPreds) {
    auto lp = TargetLaunchPolicy::initLaunchParam(ti);
    // TODO: find a way to remove this cast
    lp.requires = const_cast<ATMItaskHandle*>(predsArr);
    lp.num_required = numPreds;

    return atmi_task_launch(&lp, TargetLaunchPolicy::kernelHandle(ti), ti.argAddresses().data());
  }

 public:
  template <typename... Args>
  TaskInstance makeTask(Args&&... args) const {
    return TargetLaunchPolicy::makeTask(std::forward<Args>(args)...);
  }
};

template <typename U>
using BareTy = typename std::remove_reference<typename std::remove_cv<U>::type>::type;

template <typename __UNUSED = void>
size_t argOffsetInBuff(size_t& currBufSz, const size_t argSz, const size_t argAlign) {
  currBufSz = ((currBufSz + argAlign - 1) / argAlign) * argAlign + argSz;
  return currBufSz - argSz;
}

template <typename... Args>
std::vector<std::uint8_t> packKernArgsCompOffset(std::vector<std::size_t>& argOffsetVec,
                                                 Args&&... args) {
  // XXX: Look Ma, no loops

  size_t totBufSize = 0;

  argOffsetVec = {
      impl::argOffsetInBuff(totBufSize, sizeof(BareTy<Args>), alignof(BareTy<Args>))...};

  std::vector<std::uint8_t> argBuf(totBufSize);

  totBufSize = 0;

  // (void) to tell compiler it's a dead temp var.
  // simpler form is int[] dummy = { (0, void(expression(arg)))...}
  // FIXME: C style cast here needs to be replaced by reinterpret_cast. reinterpret_cast throws an
  // error in some cases
  (void)(void* []){
      (void*)(new (argBuf.data() +
                   impl::argOffsetInBuff(totBufSize, sizeof(BareTy<Args>), alignof(BareTy<Args>)))
                  BareTy<Args>(std::forward<Args>(args)))...};

  /*
  if (false) {
    // std::cout << "offsets in arg buf: [";
    // for (const auto &i : argOffsetVec) {
      // std::cout << i << ", ";
    // }
    // std::cout << "]" << std::endl;
  }
  */

  return argBuf;
}
} // namespace impl
} // namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_ATMI_CORE_DEF_H
