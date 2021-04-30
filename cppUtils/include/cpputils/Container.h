// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_CONTAINER_H_
#define INCLUDE_CPPUTILS_CONTAINER_H_

#include <cassert>
#include <cstdlib>

#include <vector>

namespace cpputils {


/**
 * Vector backed set
 * WARNING: does not check for duplicates upon insertion
 * User must check result of contains(x) before insert(x) or emplace(x)
 */
template <typename T, typename VecT = std::vector<T> >
struct VectorBasedSet {

  using iterator = typename VecT::iterator;
  using const_iterator = typename VecT::const_iterator;
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using size_type = typename VecT::size_type;

private:

  VecT mVec;

  template <typename I, typename V>
  static I findImpl(V& vec, const T& val) noexcept {
    I it = std::find(vec.begin(), vec.end(), val);
    return it;
  }

public:

  const_iterator find(const T& val) const noexcept {
    return findImpl<const_iterator>(mVec, val);
  }

  iterator find(const T& val) noexcept {
    return findImpl<iterator>(mVec, val);
  }

  bool contains(const T& val) const noexcept {
    return find(val) != mVec.cend();
  }

  template <typename... ArgsT>
  void emplace(ArgsT&&... args) noexcept {
    T tmp(std::forward<ArgsT>(args)...);
    assert(!contains(tmp));

    mVec.emplace_back(std::forward<ArgsT>(args)...);
  }

  void insert(const T& val) noexcept {
    assert(!contains(val));

    mVec.emplace_back(val);
  }

  void erase(const T& val) noexcept {
    assert(contains(val));

    auto it = find(val);
    if (it == mVec.end()) {
      return;
    }

    if (*it != mVec.back()) {
      std::swap(mVec.back(), *it);
    }

    mVec.pop_back();
    assert(!contains(val));
  }

  void clear() noexcept {
    mVec.clear();
  }

  bool empty() const noexcept {
    return mVec.empty();
  }

  size_type size() const noexcept {
    return mVec.size();
  }
  
  const_iterator begin() const noexcept {
    return mVec.begin();
  }

  const_iterator end() const noexcept {
    return mVec.end();
  }
};



}// end namespace cpputils


#endif//  INCLUDE_CPPUTILS_CONTAINER_H_

