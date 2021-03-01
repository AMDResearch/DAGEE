#ifndef INCLUDE_CPPUTILS_STAT_H_
#define INCLUDE_CPPUTILS_STAT_H_

#include <cassert>

#include <algorithm>
#include <vector>

namespace cpputils {

template <typename T, typename Cont_tp=std::vector<T> >
struct StatSeries {

  using container = Cont_tp;

  Cont_tp mSeries;

  void push_back(const T& val) {
    mSeries.emplace_back(val);
  }

  void clear() noexcept {
    mSeries.clear();
  }

  size_t size(void) const {
    return mSeries.size();
  }

  bool empty(void) const { 
    return mSeries.empty();
  }

  T min(void) const { 
    assert(!mSeries.empty());
    return *(std::min_element(mSeries.cbegin(), mSeries.cend()));
  }

  T max(void) const {
    assert(!mSeries.empty());
    return *(std::max_element(mSeries.cbegin(), mSeries.cend()));
  }

  std::pair<T,T> range(void) const {
    assert(!mSeries.empty());
    auto p = std::minmax_element(mSeries.cbegin(), mSeries.cend());
    return std::make_pair(*p.first, *p.second);
  }

  T sum(void) const {
    return std::accumulate(mSeries.cbegin(), mSeries.cend(), T());
  }

  template <typename U>
  U avg(void) const {
    return static_cast<U>(sum()) / static_cast<U>(size());
  }

  template <typename U>
  U stdDev(void) const {
    U a = avg();
    U sumSqDev = U();
    for (const auto& i: mSeries) {
      sumSqDev += (i - a) * (i - a);
    }
    return std::sqrt(sumSqDev);
  }

};


}// end namespace cpputils

#endif// INCLUDE_CPPUTILS_STAT_H_
