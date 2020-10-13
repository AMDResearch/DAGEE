// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ATMI_MEM_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_ATMI_MEM_EXECUTOR_H

#include "dagee/ATMIbaseExecutor.h"
#include "dagee/ATMIcoreDef.h"
#include "dagee/AllocFactory.h"

#include <cstddef>

#include <vector>

namespace dagee {

struct MemCopyInstanceAtmi {
  const void* mSrc;
  void* mDst;
  size_t mSize;
  ATMItaskHandle mATMItaskHandle;

  template <typename T>
  MemCopyInstanceAtmi(const T* src, T* dst, size_t numElems)
      : mSrc(reinterpret_cast<const void*>(src)),
        mDst(reinterpret_cast<void*>(dst)),
        mSize(numElems * sizeof(T)) {}
};

struct NoKernelRegPolicy {};

struct MemCopyTaskLaunchPolicy {
  using TaskInstance = MemCopyInstanceAtmi;

 protected:
  static ATMIcopyParam initCopyParam(const TaskInstance&) {
    return impl::initCopyParam();
  }

  static ATMItaskHandle launchInternalTask(const TaskInstance& ti,
                                           const ATMItaskHandle* predsArr,
                                           const size_t numPreds) {
    auto cp = initCopyParam(ti);

    cp.requires = const_cast<ATMItaskHandle*>(predsArr);
    cp.num_required = numPreds;

    return atmi_memcpy_async(&cp, ti.mDst, ti.mSrc, ti.mSize);
  }

  static ATMItaskHandle makeInternalTask(const TaskInstance& ti,
                                         const ATMItaskHandle* predsArr,
                                         const size_t numPreds) {
    // TODO (amber/ashwin): ATMI needs to support equivalent of atmi_task_create
    // for memcpy tasks
    return launchInternalTask(ti, predsArr, numPreds);
  }

 public:
  template <typename T>
  TaskInstance makeTask(const T* src, T* dst, size_t numElems) const {
    return TaskInstance(src, dst, numElems);
  }
};

using MemCopyExecutorAtmi =
    ExecutorSkeletonAtmi<NoKernelRegPolicy, MemCopyTaskLaunchPolicy>;

}  // end namespace dagee

#endif  // DAGEE_INCLUDE_DAGEE_ATMI_MEM_EXECUTOR_H
