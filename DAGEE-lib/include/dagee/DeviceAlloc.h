// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_DEVICE_ALLOC_H
#define DAGEE_INCLUDE_DAGEE_DEVICE_ALLOC_H

#include "dagee/AllocFactory.h"
#include "dagee/CheckStatus.h"

#include "hip/hip_common.h"
#include "hip/hip_runtime.h"

#include <algorithm>
#include <vector>

/**
 * XXX: In User Code, use DeviceBufferManager instead of standalone functions
 * here
 */

namespace dagee {

template <typename T>
inline void deviceAlloc(T*& retBuffer, const size_t numElems) {
  CHECK_HIP(hipMalloc(&retBuffer, numElems * sizeof(T)));
  assert(retBuffer && "deviceAlloc failed");
}

// Using default flags to allocate
template <typename T>
inline void hostAlloc(T*& retBuffer, const size_t numElems) {
  CHECK_HIP(hipHostMalloc(&retBuffer, numElems * sizeof(T)));
  assert(retBuffer && "hostAlloc failed");
}

template <typename T>
inline void deviceFree(T*& deviceBuffer) {
  CHECK_HIP(hipFree(deviceBuffer));
  deviceBuffer = nullptr;
}

template <typename T>
inline void hostFree(T*& hostBuffer) {
  CHECK_HIP(hipHostFree(hostBuffer));
  hostBuffer = nullptr;
}

template <typename T>
void copyToDevice(T* deviceBuffer, const T* hostBuffer, const size_t numElems) {
  CHECK_HIP(hipMemcpy(deviceBuffer, hostBuffer, numElems * sizeof(T), hipMemcpyHostToDevice));
}

template <typename T>
void copyToHost(T* hostBuffer, const T* deviceBuffer, const size_t numElems) {
  CHECK_HIP(hipMemcpy(hostBuffer, deviceBuffer, numElems * sizeof(T), hipMemcpyDeviceToHost));
}

template <typename T>
void copyHostToHost(T* destHostBuffer, const T* srcHostBuffer, const size_t numElems) {
  CHECK_HIP(hipMemcpy(destHostBuffer, srcHostBuffer, numElems * sizeof(T), hipMemcpyHostToHost));
}

template <typename AllocFactory>
class DeviceBufferManager {
  using VecBuf = typename AllocFactory::template Vec<void*>;

  VecBuf mDevBufs, mHostBufs;

  void freeAllBufs(void) {
    for (auto*& p : mDevBufs) {
      dagee::deviceFree(p);
    }
    for (auto*& p : mHostBufs) {
      dagee::hostFree(p);
    }
    mDevBufs.clear();
    mHostBufs.clear();
  }

 public:
  ~DeviceBufferManager(void) { freeAllBufs(); }

  template <typename T>
  inline T* makeDeviceCopy(const T* hostBuffer, const size_t numElems) {
    T* deviceBuffer = nullptr;
    dagee::deviceAlloc(deviceBuffer, numElems);

    dagee::copyToDevice(deviceBuffer, hostBuffer, numElems);

    assert(std::find(mDevBufs.cbegin(), mDevBufs.cend(), deviceBuffer) == mDevBufs.cend());
    mDevBufs.emplace_back(deviceBuffer);

    return deviceBuffer;
  }

  template <typename T>
  inline T* makeSharedCopy(const T* hostBuffer, const size_t numElems) {
    T* hostLockedBuffer = nullptr;
    dagee::hostAlloc(hostLockedBuffer, numElems);

    dagee::copyHostToHost(hostLockedBuffer, hostBuffer, numElems);

    assert(std::find(mHostBufs.cbegin(), mHostBufs.cend(), hostLockedBuffer) == mHostBufs.cend());
    mHostBufs.emplace_back(hostLockedBuffer);

    return hostLockedBuffer;
  }

  template <typename V, typename T = typename V::value_type>
  inline T* makeDeviceCopy(const V& vec) {
    return makeDeviceCopy(&vec[0], vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  inline T* makeSharedCopy(const V& vec) {
    return makeSharedCopy(&vec[0], vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  void copyToDevice(T* deviceBuffer, const V& vec) {
    dagee::copyToDevice(deviceBuffer, &vec[0], vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  void copyToHost(V& vec, const T* deviceBuffer) {
    dagee::copyToHost(&vec[0], deviceBuffer, vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  void copyToHostVector(V& vec, const T* hostBuffer) {
    dagee::copyHostToHost(&vec[0], hostBuffer, vec.size());
  }
};

using DeviceBufMgr = DeviceBufferManager<dagee::StdAllocatorFactory<> >;

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_DEVICE_ALLOC_H
