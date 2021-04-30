// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__HSA_CORE_H_
#define DAGEE_INCLUDE_DAGR__HSA_CORE_H_

#include "cpputils/Heap.h"
#include "cpputils/Allocator.h"

#include <hsa/hsa.h>
#include <hsa/hsa_ext_amd.h>

#include <cassert>
#include <cstddef>
#include <cstdio>


#include <vector>
#include <string>

#define ASSERT_HSA(cmd) \
  do { \
    hsa_status_t cmd_status = (cmd); \
    (cmd_status); \
    assert(cmd_status == HSA_STATUS_SUCCESS && "HSA Command" #cmd "failed"); \
  } while(false);


namespace dagr {

using Agent = hsa_agent_t;
using Signal = hsa_signal_t;
using Region = hsa_region_t;
using KernelDispatchPkt = hsa_kernel_dispatch_packet_t;
using BarrierAndPkt = hsa_barrier_and_packet_t;
using BarrierOrPkt = hsa_barrier_or_packet_t;
using HsaQueue = hsa_queue_t;
using CodeObjectReader = hsa_code_object_reader_t;
using Executable = hsa_executable_t;

enum class DeviceKind: std::underlying_type<hsa_device_type_t>::type {
  GPU = HSA_DEVICE_TYPE_GPU,
  CPU = HSA_DEVICE_TYPE_CPU
};

struct KernelObject {
  uint64_t mVal = 0ul;

  bool isValid() const noexcept {
    return mVal != 0ul;
  }
};

enum class PacketKind: std::underlying_type<hsa_packet_type_t>::type {
  KERNEL_DISPATCH = HSA_PACKET_TYPE_KERNEL_DISPATCH,
  BARRIER_AND = HSA_PACKET_TYPE_BARRIER_AND,
  BARRIER_OR = HSA_PACKET_TYPE_BARRIER_OR
};

enum class FenceScope: std::underlying_type<hsa_fence_scope_t>::type {
  NONE = HSA_FENCE_SCOPE_NONE,
  AGENT = HSA_FENCE_SCOPE_AGENT,
  SYSTEM = HSA_FENCE_SCOPE_SYSTEM
};

enum class BarrierBit: uint16_t { // same as paket header size which is 16 bBit
  DISABLE = 0u,
  ENABLE = 1u
};

class PacketHeader {
  uint16_t mVal = 0u;

  template <typename EnumT>
  static auto toUnderTy(const EnumT& val) noexcept -> typename std::underlying_type<EnumT>::type {
    return static_cast<typename std::underlying_type<EnumT>::type>(val);
  }

  void setKind(const PacketKind& kind) noexcept {
    mVal |= toUnderTy(kind) << HSA_PACKET_HEADER_TYPE;
  }

  void setScope(const FenceScope& scope) noexcept {
    mVal |= toUnderTy(scope) << HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE;
    mVal |= toUnderTy(scope) << HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE;
  }

  void setBarrierBit(const BarrierBit& bBit) noexcept {
    mVal |= toUnderTy(bBit) << HSA_PACKET_HEADER_BARRIER;
  }

  void reset() noexcept {
    mVal = 0u;
  }

public:

  uint16_t value() const noexcept {
    return mVal;
  }

  PacketHeader(const PacketKind& kind, const FenceScope& scope, const BarrierBit& bBit) noexcept {
    setKind(kind);
    setScope(scope);
    setBarrierBit(bBit);
  }

};


struct AgentEqualsFunctor {
  const Agent& mAgent;

  bool operator () (const Agent& that) const noexcept {
    return mAgent.handle == that.handle;
  }
};

class AgentQuery {

  static size_t sizeTyAttr(const Agent& agent, const hsa_agent_info_t& attrName) noexcept {
    size_t sz = 0ul;
    ASSERT_HSA(hsa_agent_get_info(agent, attrName, &sz));
    assert(sz != 0ul);
    return sz;
  }

public:

  static std::string name(const Agent& agent) noexcept {
    char ret[64] = {0};
    ASSERT_HSA(hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, ret));
    return ret;
  }

  static DeviceKind deviceKind(const Agent& agent) noexcept {
    hsa_device_type_t devKind;
    ASSERT_HSA(hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &devKind));
    return static_cast<DeviceKind>(devKind);
  }

  static size_t maxQueueSize(const Agent& agent) noexcept {
    return sizeTyAttr(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE);
  }

  static size_t minQueueSize(const Agent& agent) noexcept {
    return sizeTyAttr(agent, HSA_AGENT_INFO_QUEUE_MIN_SIZE);
  }

  static size_t maxNumQueues(const Agent& agent) noexcept {
    return sizeTyAttr(agent, HSA_AGENT_INFO_QUEUES_MAX);
  }

  static size_t wavefrontSize(const Agent& agent) noexcept {
    return sizeTyAttr(agent, HSA_AGENT_INFO_WAVEFRONT_SIZE);
  }

};


struct SignalFactoryBase {

  static void destroy(const Signal& s) noexcept {
    ASSERT_HSA(hsa_signal_destroy(s));
  }
};

struct InterruptSignalFactory: public SignalFactoryBase {
  static Signal create() noexcept {
    Signal s;
    ASSERT_HSA(hsa_signal_create(1, 0, nullptr, &s));
    return s;
  }
};

struct UserSignalFactory: public SignalFactoryBase {
  static Signal create() noexcept {
    Signal s;
    ASSERT_HSA(hsa_amd_signal_create(1, 0, nullptr, HSA_AMD_SIGNAL_AMD_GPU_ONLY, &s));
    return s;
  }
};

struct IpcSignalFactory: public SignalFactoryBase {
  static Signal create() noexcept {
    Signal s;
    ASSERT_HSA(hsa_amd_signal_create(1, 0, nullptr, HSA_AMD_SIGNAL_IPC, &s));
    return s;
  }
};

namespace impl {
  constexpr static const size_t INTR_SIGNAL_POOL_BATCH_SIZE = 4096ul;
  constexpr static const size_t USER_SIGNAL_POOL_BATCH_SIZE = 16 * 4096ul;

  constexpr static const Signal NULL_SIGNAL = {0};
}

bool operator == (const Signal& left, const Signal& right) {
  return left.handle == right.handle;
}

bool operator != (const Signal& left, const Signal& right) {
  return left.handle != right.handle;
}


using InterruptSignalPool = 
  cpputils::ResourcePoolStaticFactory<Signal, InterruptSignalFactory, impl::INTR_SIGNAL_POOL_BATCH_SIZE>;
using UserSignalPool = 
  cpputils::ResourcePoolStaticFactory<Signal, UserSignalFactory, impl::USER_SIGNAL_POOL_BATCH_SIZE>;

struct SignalPoolState {
  UserSignalPool mUserSignalPool;
  InterruptSignalPool mIntrptSignalPool;
  

  Signal takeUserSignal() noexcept {
    return mUserSignalPool.allocate();
  }

  void returnUserSignal(const Signal& sig) noexcept {
    mUserSignalPool.deallocate(sig);
  }

  Signal takeIntrptSignal() noexcept {
    return mIntrptSignalPool.allocate();
  }

  void returnIntrptSignal(const Signal& sig) noexcept {
    mIntrptSignalPool.deallocate(sig);
  }
};

/*
template <typename T, typename DeleteFunc>
struct AutoDeleteResource {
  T mObj;
  DeleteFunc mDelFunc;

  AutoDeleteResource(const AutoDeleteResource&) = delete;
  AutoDeleteResource& operator = (const AutoDeleteResource&) = delete;

  

};
*/

struct InitHsa {
  InitHsa() noexcept {
    ASSERT_HSA(hsa_init());
  }
  ~InitHsa() noexcept {
    ASSERT_HSA(hsa_shut_down());
  }
};



class RuntimeState {

  using VecAgent = std::vector<Agent>;

  InitHsa mInit;
  VecAgent mGpuAgents;
  VecAgent mCpuAgents;

  static void checkDuplicate(const VecAgent& agentVec, const Agent& agent) noexcept {
    assert(std::find_if(agentVec.cbegin(), agentVec.cend(), AgentEqualsFunctor{agent}) == agentVec.cend());

  }

  static Agent getAgent(const VecAgent& agentVec, size_t id) noexcept {
    return agentVec.at(id);
  }

  void addNewAgent(Agent agent, const hsa_device_type_t& deviceKind) noexcept {

    switch (deviceKind) {
      case HSA_DEVICE_TYPE_CPU:
        checkDuplicate(mCpuAgents, agent);
        mCpuAgents.emplace_back(agent);
        break;
      case HSA_DEVICE_TYPE_GPU:
        checkDuplicate(mGpuAgents, agent);
        mGpuAgents.emplace_back(agent);
        break;
      default:
        std::abort();
    }
  }

  static hsa_status_t findGpuAgents(hsa_agent_t agent, void* data) {
    if (!data) {
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    hsa_device_type_t deviceKind;
    ASSERT_HSA(hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &deviceKind));

    auto* outer = reinterpret_cast<RuntimeState*>(data);

    outer->addNewAgent(agent, deviceKind);

    return HSA_STATUS_SUCCESS;
  }

  void init() noexcept {
    ASSERT_HSA(hsa_iterate_agents(findGpuAgents, this));
    assert(!mGpuAgents.empty());
    assert(!mCpuAgents.empty());
  }

public:

  RuntimeState() noexcept {
    init();
  }

  Agent gpuAgent(size_t id) const noexcept {
    return getAgent(mGpuAgents, id);
  }

  Agent cpuAgent(size_t id) const noexcept {
    return getAgent(mCpuAgents, id);
  }
};

namespace impl {

  template <typename __UnusedT=void>
  void waitOnSignalAcquire(const Signal& sig) noexcept {
    while (hsa_signal_wait_scacquire(sig, HSA_SIGNAL_CONDITION_EQ, 0, 0, HSA_WAIT_STATE_ACTIVE) != 0) {
      // std::printf("Waiting on signal, value = %ld\n", hsa_signal_load_relaxed(sig));
    }
  }

  template <typename __UnusedT=void>
  void waitOnSignalRelaxed(const Signal& sig) noexcept {
    while (hsa_signal_wait_relaxed(sig, HSA_SIGNAL_CONDITION_EQ, 0, 0, HSA_WAIT_STATE_ACTIVE) != 0) {
      // std::printf("Waiting on signal, value = %ld\n", hsa_signal_load_relaxed(sig));
    }
  }

  template <typename __UnusedT=void>
  void waitOnSignalBusy(const Signal& sig) noexcept {
    while (hsa_signal_load_relaxed(sig) != 0) {
      // std::printf("Waiting on signal, value = %ld\n", hsa_signal_load_relaxed(sig));
    }
  }
}// end namespace impl


}// end namespace dagr

#endif// DAGEE_INCLUDE_DAGR__HSA_CORE_H_
