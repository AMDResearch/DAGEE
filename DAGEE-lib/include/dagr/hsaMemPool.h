// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef HSA_MEM_POOL_H_
#define HSA_MEM_POOL_H_

struct AgentMemPoolState {

  using VecMemPool = std::vector<MemPool>;

  Agent mAgent;

  VecMemPool mKernargPools;
  VecMemPool mCoarsegrainPools;
  VecMemPool mFinegrainPools;

  void registerMemPool(const MemPool& pool, const hsa_pool_global_flag_t& flags) noexcept {

    if (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT) {

      mKernargPools.emplace_back(pool);

    } else if (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED) {

      mCoarsegrainPools.emplace_back(pool);

    } else if (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED) {

      mFinegrainPools.emplace_back(pool);

    } else {
      std::abort();
    }
  }

  hsa_status_t static visitMemPool(hsa_amd_memory_pool_t pool, void* data) noexcept {
    if(!data) {
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    hsa_amd_segment_t segment;
    ASSERT_HSA(hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &segment));
    
    if (segment != HSA_AMD_SEGMENT_GLOBAL) { 
      return HSA_STATUS_SUCCESS;
    }

    hsa_amd_memory_pool_global_flag_t flags;
    ASSERT_HSA(hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags));

    AgentMemPoolState* outer = reinterpret_cast<AgentMemPoolState*>(data);
    assert(outer);
    outer->registerMemPool(pool, flags);

    return HSA_STATUS_SUCCESS;
  }
  
  void init() noexcept {
    ASSERT_HSA(hsa_amd_agent_iterate_memory_pools(mAgent, visitMemPool, this));
  }

public:

  explicit AgentMemPoolState(Agent a) noexcept: mAgent(a) {
    init();
  }
  
};


struct MemPoolQuery {

private:
  static size_t sizeTyAttr(const MemPool& pool, const hsa_amd_memory_pool_info_t& attrName) noexcept {
    size_t sz = 0ul;
    ASSERT_HSA(hsa_amd_memory_pool_get_info(pool, attrName, &sz));
    assert(sz != 0ul);
    return sz;
  }

public:

  static size_t size(const MemPool& pool) noexcept {
    return sizeTyAttr(pool, HSA_AMD_MEMORY_POOL_INFO_SIZE);
  }

  static size_t allocGranule(const MemPool& pool) noexcept {
    return sizeTyAttr(pool, HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_GRANULE);
  }

  static size_t allocAlignment(const MemPool& pool) noexcept {
    return sizeTyAttr(pool, HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALIGNMENT);
  }

  static size_t allocMaxSize(const MemPool& pool) noexcept {
    return sizeTyAttr(pool, HSA_AMD_MEMORY_POOL_INFO_ALLOC_MAX_SIZE);
  }
};

#endif// HSA_MEM_POOL_H_
