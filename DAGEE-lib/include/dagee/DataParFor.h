// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_DATA_PAR_FOR_H
#define DAGEE_INCLUDE_DAGEE_DATA_PAR_FOR_H

#include "hip/hip_runtime.h"

#include <vector>

namespace dagee {

template <typename I>
struct Range1D {
  I mbeg;
  I mend;

  I begin() const { return mbeg; }
  I end() const { return mend; }
  ptrdiff_t size() const { return mend - mbeg; }

  friend operator+(const Range1D& r1, const Range1D& r2) {
    I beg = std::min(r1.mbeg, r2.mbeg);
    I end = beg + (r1.size() + r2.size());
  }
};

template <typename I>
Range1D<I> iterate(const I& b, const I& e) {
  return Range1D<I>{b, e};
}

template <typename R, typename F>
struct DataParForInstance {
  R mrange;
  F mfunc;
};

template <typename R, typename F>
struct MergedDataParForInstance {};

namespace impl {
template <typename R, typename F>
__global__ void dataParForImpl(R range, F func) {
  size_t tid = hipBlockDim_x * hipBlockIdx_x + hipThreadIdx_x;
  auto i = range.begin() + tid;

  if (i < range.end()) {
    func(i);
  }
}
} // namespace impl

template <typename R, typename F>
void data_par_for(const R& range, const F& func) {
  constexpr const size_t NUM_BLOCKS = 8;

  const size_t threadsPerBlock = (range.end() - range.begin() + NUM_BLOCKS - 1) / NUM_BLOCKS;

  hipLaunchKernelGGL(&impl::dataParForImpl, dim3(NUM_BLOCKS), dim3(threadsPerBlock), 0, 0, range,
                     func);
}

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_DATA_PAR_FOR_H
