// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ATMI_DEVICE_ALLOC_H
#define DAGEE_INCLUDE_DAGEE_ATMI_DEVICE_ALLOC_H

// TODO(amber): rename to MemAllocAtmi.h

#include "dagee/ATMIcoreDef.h"
#include "dagee/AllocFactory.h"
#include "dagee/CheckStatus.h"

#include "atmi.h"
#include "atmi_runtime.h"

#include <algorithm>
#include <vector>

namespace dagee {

template <typename T>
inline void atmiMalloc(MemType memType, T*& retBuffer, const size_t numElems) {
  CHECK_ATMI(atmi_malloc(reinterpret_cast<void**>(&retBuffer), numElems * sizeof(T),
                         impl::initMemPlaceAtmi(memType)));
  assert(retBuffer && "Malloc failed");
}

template <typename T>
inline void atmiFree(T*& buffer) {
  CHECK_ATMI(atmi_free(buffer));
  buffer = nullptr;
}

template <typename T>
inline void atmiMemcpy(T* dstBuffer, const T* srcBuffer, const size_t numElems) {
  CHECK_ATMI(atmi_memcpy(reinterpret_cast<void*>(dstBuffer),
                         reinterpret_cast<const void*>(srcBuffer), numElems * sizeof(T)));
}
template <typename AllocFactory = dagee::StdAllocatorFactory<> >
class AllocManagerAtmiImpl {
  using VecBuf = typename AllocFactory::template Vec<void*>;

  VecBuf mBufs;

  void freeAllBufs(void) {
    for (auto*& p : mBufs) {
      dagee::atmiFree(p);
    }
    mBufs.clear();
  }

  template <typename T, typename V>
  T* makeCopyImpl(const V& vec, MemType memType) {
    T* retBuffer = nullptr;
    atmiMalloc(memType, retBuffer, vec.size());
    atmiMemcpy(retBuffer, &vec[0], vec.size());

    assert(std::find(mBufs.cbegin(), mBufs.cend(), retBuffer) == mBufs.cend() &&
           "duplicate buffer");
    mBufs.emplace_back(retBuffer);

    return retBuffer;
  }

 public:
  ~AllocManagerAtmiImpl(void) { freeAllBufs(); }

  template <typename V, typename T = typename V::value_type>
  T* makeDeviceCopy(const V& vec) {
    return makeCopyImpl<T>(vec, MemType::GPU);
  }

  template <typename V, typename T = typename V::value_type>
  T* makeSharedCopy(const V& vec) {
    return makeCopyImpl<T>(vec, MemType::SHARED);
  }

  template <typename V, typename T = typename V::value_type>
  void copyVecToBuffer(T* buffer, const V& vec) const {
    dagee::atmiMemcpy(buffer, &vec[0], vec.size());
  }

  template <typename V, typename T = typename V::value_type>
  void copyBufferToVec(V& vec, const T* buffer) const {
    dagee::atmiMemcpy(&vec[0], buffer, vec.size());
  }
};

using AllocManagerAtmi = AllocManagerAtmiImpl<>;

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_ATMI_DEVICE_ALLOC_H
