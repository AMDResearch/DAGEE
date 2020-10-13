// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_HIP_SYNC_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_HIP_SYNC_EXECUTOR_H

#include "dagee/AllocFactory.h"
#include "dagee/DeviceAlloc.h"
#include "dagee/DummyKernel.h"

#include "util/Print.h"

#include "hip/hip_runtime.h"

#include <string>

namespace dagee {

template <typename AllocFactory = dagee::StdAllocatorFactory<> >
struct HIPsyncExecutor: public DeviceBufferManager<AllocFactory> {

  using DeviceAllocBase = DeviceBufferManager<AllocFactory>;


  size_t mNumTasks;
  std::string mName;

  template <typename S>
  explicit HIPsyncExecutor(const S& name): 
    DeviceAllocBase(),
    mNumTasks(0),
    mName(name)
  {}

  ~HIPsyncExecutor(void) {
    util::printStat(mName, "Num Tasks", mNumTasks);
  }

  template <typename F, typename... Args>
  void launchSyncTask(const dim3& blocks, const dim3& threadsPerBlock, const F& f, Args... args) {
    hipLaunchKernelGGL(f, blocks, threadsPerBlock, 0, 0, args...);
    ++mNumTasks;
  }

  void syncWithDevice(void) {
    hipDeviceSynchronize();
  }

  void launchDummyKernel(void) const {
    dagee::launchDummyKernel();
  }
};

} // end namespace dagee


#endif// DAGEE_INCLUDE_DAGEE_HIP_SYNC_EXECUTOR_H
