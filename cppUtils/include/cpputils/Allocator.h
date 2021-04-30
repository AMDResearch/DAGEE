// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_ALLOCATOR_H_
#define INCLUDE_CPPUTILS_ALLOCATOR_H_

#include <cassert>
#include <cstdlib>

#include <memory>
#include <vector>
#include <type_traits>

namespace cpputils {

template <typename T>
class AlignedAllocator : public std::allocator<T> {
  using Base = std::allocator<T>;

 protected:
  size_t mAlign;

 public:
  explicit AlignedAllocator(size_t alignment) noexcept : Base(), mAlign(alignment) {
    assert((mAlign & 3ul) == 0 && "lowest alignment must be at least 4 bytes");
  }

  T* allocate(size_t n) const noexcept { return aligned_alloc(mAlign, n); }

  T* allocate(size_t n) override { return const_cast<const AlignedAllocator*>(this)->allocate(n); }

  void deallocate(T* ptr, size_t n) const noexcept { free(ptr); }

  void deallocate(T* ptr, size_t n) override {
    const_cast<const AlignedAllocator*>(this)->deallocate(ptr, n);
  }

  size_t alignment(void) const noexcept { return mAlign; }
};


/**
 * generic resource pool that holds some number of objects of type T
 * Objects are created via a static factory class that exposes create and
 * destroy methods that work on one object of type T. 
 */

template <typename T, typename FactoryT, size_t BATCH_SIZE_T>
struct ResourcePool {
  constexpr static const size_t BATCH_SIZE = BATCH_SIZE_T;
  using value_type = T;

private:

  using Pool = std::vector<T>;

  FactoryT& mFactory;
  Pool mResPool;


  void replenish() noexcept {
    assert(mResPool.empty());

    for (size_t i = 0; i < BATCH_SIZE; ++i) {
      mResPool.emplace_back(mFactory.create());
    }
  }

  void init() noexcept {
    // TODO: add policy on whether to replenish at creation/init or not
    replenish();
  }

  void destroy() noexcept {
    for (const auto& r: mResPool) {
      mFactory.destroy(r);
    }
    mResPool.clear();
  }

public:

  explicit ResourcePool(FactoryT& factory) noexcept: 
    mFactory(factory),
    mResPool()
  {
    init();
  }

  ~ResourcePool() noexcept {
    destroy();
  }

  T allocate() noexcept {
    if (mResPool.empty()) {
      replenish();
    }

    assert(!mResPool.empty());

    auto obj = mResPool.back();
    mResPool.pop_back();
    return obj;
  }

  void deallocate(const T& obj) {
    mResPool.push_back(obj);
  }
};

namespace impl {
  template <typename T, typename StaticFactoryT>
  struct StaticFactoryWrapper {
    
    T create() const noexcept {
      return StaticFactoryT::create();
    }

    void destroy(const T& obj) const noexcept {
      StaticFactoryT::destroy(obj);
    }
  };
}

template <typename T, typename StaticFactoryT, size_t BATCH_SIZE_T>
struct ResourcePoolStaticFactory: public ResourcePool<T, impl::StaticFactoryWrapper<T, StaticFactoryT>, BATCH_SIZE_T> {
  using SFW = impl::StaticFactoryWrapper<T, StaticFactoryT>;
  using Base = ResourcePool<T, SFW, BATCH_SIZE_T>;
  SFW mStaticFactory;

  // FIXME: passing a reference to mStaticFactory before it is constructed
  ResourcePoolStaticFactory(): Base(mStaticFactory), mStaticFactory() {}
};


} // end namespace cpputils

#endif // INCLUDE_CPPUTILS_ALLOCATOR_H_
