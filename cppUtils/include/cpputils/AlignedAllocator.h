// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_ALIGNED_ALLOCATOR_H_
#define INCLUDE_CPPUTILS_ALIGNED_ALLOCATOR_H_

#include <cstdlib>
#include <cassert>

#include <memory>

namespace cpputils {

template <typename T>
class AlignedAllocator: public std::allocator<T> {
  using Base = std::allocator<T>;

protected:

  size_t mAlign;

public:

  explicit AlignedAllocator(size_t alignment) noexcept: 
    Base(),
    mAlign(alignment) 
  {
    assert(mAlign & 3 == 0 && "lowest alignment must be at least 4 bytes");
  }

  T* allocate(size_t n) const noexcept {
    return aligned_alloc(mAlign, n);
  }

  T* allocate(size_t n) override {
    return const_cast<const AlignedAllocator*>(this)->allocate(n);
  }

  void deallocate(T* ptr, size_t n) const noexcept {
    free(ptr);
  }

  void deallocate(T* ptr, size_t n) override {
    const_cast<const AlignedAllocator*>(this)->deallocate(ptr, n);
  }

  size_t alignment(void) const noexcept {
    return mAlign;
  }

};

}// end namespace cpputils

#endif// INCLUDE_CPPUTILS_ALIGNED_ALLOCATOR_H_
