// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ALLOC_FACTORY_H
#define DAGEE_INCLUDE_DAGEE_ALLOC_FACTORY_H

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dagee {

/**
 * Goal: We want to use custom allocators everywhere, with all containers for
 * 1. Scalability in parallel execution
 * 2. Portability to other memory spaces
 *
 * Implementation Choices:
 *
 * A. Most general:
 * We pass an instance of AllocatorFactory which has methods and typedefs for
 * creating allocators and STL containers with allocators. We do not assume that
 * allocators are default constructible.
 *
 * Positives:
 * 1. No assumptions on how allocators are constructed, i.e.
 *
 * The downsides are in code complexity:i
 * 1. we have to pass around and keep an instance of AllocatorFactory,
 * 2. STL containers are not default constructible due to allocators not being
 * default constructible.
 *
 * B. Most convenient:
 * We keep AllocatorFactory as a static class which has typedefs and static methods for
 * custom allocators and STL containers (with custom allocators). However, all
 * allocators are default constructible.
 *
 * Positives:
 * 1. We don't need to pass around AllocatorFactory reference. Only the name of
 * AllocatorFactory class. So there's a std AllocatorFactory and
 * custom-alloc-factory AllocatorFactory etc. All we need is to change the type of AllocatorFactory
 *
 * Negatives:
 * 1. Static initialization. The AllocatorFactory is built on top of some heap, which is maintained
 * as static variables and statically initialized. Some Allocators may be multi-threaded so they
 * have a thread-local heap. Multi-threaded static initialization can have its pitfalls.
 *
 * 2. Sometimes we may want to create an allocator over a specific region, or in
 * general create an allocator with non-default constructor
 *
 * COMPROMISE
 * - We can have default constructible allocators as well as static makeAlloc functions
 *   that create allocators with non-default constructors.
 */

using Byte = uint8_t;
using BytePtr = uint8_t*;

template <typename __UNUSED = void>
struct StdAllocatorFactory {
  template <typename T>
  using FixedSizeAlloc = std::allocator<T>;

  template <typename T>
  using VarSizeAlloc = std::allocator<T>;

  template <typename T>
  using Vec = std::vector<T, VarSizeAlloc<T> >;

  using Str = std::basic_string<char, std::char_traits<char>, VarSizeAlloc<char> >;

  template <typename T>
  using Deque = std::deque<T, VarSizeAlloc<T> >;

  template <typename K, typename V, typename H = std::hash<K>, typename EQ = std::equal_to<K> >
  using HashMap = std::unordered_map<K, V, H, EQ, VarSizeAlloc<std::pair<const K, V> > >;

  // TODO: add more stl containers such as set, map, unordered_set, unordered_map

  template <typename T>
  static FixedSizeAlloc<T> makeFixedSizeAlloc(void) {
    return FixedSizeAlloc<T>();
  }

  template <typename T>
  static VarSizeAlloc<T> makeVarSizeAlloc(void) {
    return VarSizeAlloc<T>();
  }

  template <typename T>
  static Vec<T> makeVec(void) {
    return Vec<T>(makeVarSizeAlloc<T>());
  }

  template <typename T>
  static Deque<T> makeDeque(void) {
    return Deque<T>(makeVarSizeAlloc<T>());
  }
};

template <typename A>
using AllocTraits = std::allocator_traits<A>;

template <typename A, typename T>
using RebindAlloc = typename dagee::AllocTraits<A>::template rebind_alloc<T>::type;

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_ALLOC_FACTORY_H
