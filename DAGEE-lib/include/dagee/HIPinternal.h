// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_HIP_INTERNAL_H
#define DAGEE_INCLUDE_DAGEE_HIP_INTERNAL_H

#include "dagee/HIPdefs.h"
#include "dagee/coreDef.h"

#include "hip/hcc_detail/program_state.hpp"
#include "hip/hip_runtime.h"

#include <type_traits>

namespace dagee {

struct KernelInfoHSA {
  std::uint64_t mCodeObj;
  const amd_kernel_code_t* mKernHeader;
  hsa_agent_t mAgent;

 public:
  size_t privateSegmentSize() const { return mKernHeader->workitem_private_segment_byte_size; }

  size_t groupSegmentSize() const { return mKernHeader->workgroup_group_segment_byte_size; }

  std::uint64_t codeObjAddr() const { return mCodeObj; }
};

namespace impl {

using KernDesc = hip_impl::Kernel_descriptor;

template <typename __UNUSED = void>
KernelInfoHSA getKernelInfoHSA(KernelFuncPtrStorage Fptr) {
  // A compile-time reflection of hip_impl::Kernel_descriptor
  // Ugly hack to access private members
  struct KernDescMirror {
    // Members here are
    uint64_t mObject = 0;
    const amd_kernel_code_t* mHeader = nullptr;
    char _mDont_use[sizeof(std::string)];

    KernDescMirror(const KernDesc& kernDesc) { std::memcpy(this, &kernDesc, sizeof(KernDesc)); }
  };

  auto it = hip_impl::functions().find(Fptr);

  const auto& vec = it->second;

  // for (const auto& p: vec) {
  //   std::cout << "HSA agent: " << p.first.handle << std::endl;
  //   std::cout << "Kernel_descriptor: " << p.second << std::endl;
  // }

  // FIXME: this can't handle multiple GPUs (esp. if they are different
  // architectures)
  hsa_agent_t agent = vec[0].first;
  const auto& kernDesc = vec[0].second;

  KernDescMirror mirror(kernDesc);

  return KernelInfoHSA{mirror.mObject, mirror.mHeader, agent};
}

template <typename __UNUSED = void>
size_t nextBufSize(const size_t origSize, const size_t sz, const size_t algn) {
  return ((origSize + algn - 1) / algn) * algn + sz;
}

template <unsigned I, unsigned N, bool B>
struct FillArgBuf {
  template <typename T, typename V>
  static void go(V& vec, const T& tup) {}
};

template <unsigned I, unsigned N>
struct FillArgBuf<I, N, true> {
  template <typename V, typename T>
  static void go(V& vec, const T& tup) {
    auto obj_i = std::get<I>(tup);
    using X = typename std::remove_reference<typename std::remove_cv<decltype(obj_i)>::type>::type;

    size_t newSz = nextBufSize(vec.size(), sizeof(X), alignof(X));
    vec.resize(newSz);

    new (vec.data() + vec.size() - sizeof(X)) X(obj_i);

    FillArgBuf<I + 1, N, ((I + 1) < N)>::go(vec, tup);
  }
};

template <typename... Args>
std::vector<std::uint8_t> packKernArgs(Args&&... args) {
  auto tup = std::make_tuple(std::forward<Args>(args)...);
  using Tup_ty = decltype(tup);

  std::vector<std::uint8_t> argBuf;

  constexpr const size_t N = std::tuple_size<Tup_ty>::value;
  FillArgBuf<0, N, 0 < N>::go(argBuf, tup);

  return argBuf;
}

} // end namespace impl

// FIXME: Args to kernel instance must be passed by copy

template <typename KI>
struct KernelInstance {
  dim3 mBlocks;
  dim3 mThreadsPerBlock;
  KI mKernInfo;
  std::vector<std::uint8_t> mKernArgs;

  template <typename... Args>
  KernelInstance(const dim3& blocks, const dim3& threadsPerBlock, const KI& kinfo, Args&&... args)
      : mBlocks(blocks),
        mThreadsPerBlock(threadsPerBlock),
        mKernInfo(kinfo),
        mKernArgs(impl::packKernArgs(std::forward<Args>(args)...)) {}
};

template <typename KI, typename... Args>
KernelInstance<KI> makeKernelInstance(const dim3& blocks, const dim3& threadsPerBlock,
                                      const KI& kinfo, Args&&... args) {
  return KernelInstance<KI>(blocks, threadsPerBlock, kinfo, std::forward<Args>(args)...);
}

template <typename... Args, typename FuncPtr = void (*)(Args...)>
KernelInstance<KernelInfoHSA> makeHSAkernelInstance(const dim3& blocks, const dim3& threadsPerBlock,
                                                    FuncPtr kernPtr, Args&&... args) {
  auto kptr = reinterpret_cast<KernelFuncPtrStorage>(kernPtr);
  auto kinfo = impl::getKernelInfoHSA(kptr);

  return KernelInstance<KernelInfoHSA>(blocks, threadsPerBlock, kinfo, std::forward<Args>(args)...);
}

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_HIP_INTERNAL_H
