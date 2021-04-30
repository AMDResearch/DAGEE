// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__EXECUTOR_H_
#define DAGEE_INCLUDE_DAGR__EXECUTOR_H_

#include "dagr/hsaCore.h"
#include "dagr/binary.h"
#include "dagr/kernel.h"
#include "dagr/queue.h"


#include <unordered_map>
#include <vector>

namespace dagr {



class GpuExecutionResource {

  Agent mAgent;
  AgentRegionState mRegionState;
  KernArgHeap mKernArgHeap;
  ExecutableState mBinaryState;
  GpuKernInfoState mKernInfoState;
  HsaQueuePoolState mQueuePoolState;
  SignalPoolState mSignalPoolState;

public:

  explicit GpuExecutionResource(const Agent& agent) noexcept:
    mAgent(agent),
    mRegionState(mAgent),
    mKernArgHeap(mRegionState.kernArgRegion()),
    mBinaryState(mAgent),
    mKernInfoState(mBinaryState, mKernArgHeap),
    mQueuePoolState(mAgent),
    mSignalPoolState()
  {
    assert(AgentQuery::deviceKind(mAgent) == DeviceKind::GPU);
  }


  template <typename... Args>
  GpuKernInstance makeTask(const dim3& blks, const dim3& thrdsPerBlk, const GpuKernInfo* kinfo, Args&&... args) noexcept {

    assert(kinfo);

    if (kinfo->kernArgBufSize() == 0) {
      return GpuKernInstance(blks, thrdsPerBlk, kinfo, MemBlock {nullptr, nullptr}, std::forward<Args>(args)...);
    }
    
    // FIXME: find a way to remove const_cast
    MemBlock argBuf = const_cast<GpuKernInfo*>(kinfo)->kernArgHeap()->allocate();
    assert(argBuf.size() >= kinfo->kernArgBufSize());
    return GpuKernInstance(blks, thrdsPerBlk, kinfo, argBuf, std::forward<Args>(args)...);
  }

  SignalPoolState& signalPool() noexcept {
    return mSignalPoolState;
  }

  ExecutableState& binaryState() noexcept {
    return mBinaryState;
  }

  HsaQueuePoolState& queuePool() noexcept {
    return mQueuePoolState;
  }

  GpuKernInfoState& kernInfoState() noexcept {
    return mKernInfoState;
  }

  AgentRegionState& regionState() noexcept {
    return mRegionState;
  }

};

// FIXME: Signals are not being returned correctly. What if user never calls wait
// on a task handle. What if we are using signals for something else such as
// barrier packets
struct TaskHandle {
  Signal mSignal;
  explicit TaskHandle(const Signal& sig): mSignal(sig) {}
};


struct SerialGpuExecutorBase {

  GpuExecutionResource* mExecRsrc;


  /*
  template <typename F, typename... Args, typename K = void (*)(Args...)>
  GpuKernInfo* registerKernel(F func) {
    static_assert(std::is_same<F, K>::value, "Invalid kernel function pointer");
  }
  */

  GpuExecutionResource& execResource() noexcept { 
    assert(mExecRsrc);
    return *mExecRsrc;
  }

  template <typename... Args>
  const GpuKernInfo* registerKernel(const char* kname) noexcept {
    return execResource().kernInfoState().template registerKernel<Args...>(kname);
  }

  template <typename... Args>
  GpuKernInstance makeTask(const dim3& blks, const dim3& thrdsPerBlk, const GpuKernInfo* kinfo, Args&&... args) noexcept  {

    return execResource().makeTask(blks, thrdsPerBlk, kinfo, std::forward<Args>(args)...);

  }

  void addTaskToQ(DispatchQueueSerial& dispatchQ, const GpuKernInstance& ki, const PacketHeader& header, const Signal& compSig) noexcept {

    AqlPacket* pkt = dispatchQ.giveOneSlot();
    assert(pkt);

    PacketFactory::init(reinterpret_cast<KernelDispatchPkt*>(pkt), header, ki, compSig);
  }

  void waitOnTask(const TaskHandle& th) noexcept {
    impl::waitOnSignalRelaxed(th.mSignal);
    //impl::waitOnSignalBusy(th.mSignal);
    // impl::waitOnSignalAcquire(th.mSignal);
    // FIXME: figure out the right logic to return signals efficiently
    //hsa_signal_store_relaxed(th.mSignal, 1);
    // execResource().signalPool().returnUserSignal(th.mSignal);
  }
  
  explicit SerialGpuExecutorBase(GpuExecutionResource* e) noexcept: 
    mExecRsrc(e)
  {
    assert(e);
  }

};

struct SerialOrderedExecutor: public SerialGpuExecutorBase {

  using Base = SerialGpuExecutorBase;

  DispatchQueueSerial mDispQueue;

  explicit SerialOrderedExecutor(GpuExecutionResource* e) noexcept:
    Base(e),
    mDispQueue(execResource().queuePool().takeSerialQueue())
  {}

  ~SerialOrderedExecutor() {
    execResource().queuePool().returnSerialQueue(mDispQueue.hsaQueue());
  }

  void addTaskImpl(const GpuKernInstance& ki, const Signal& compSig, const FenceScope& scope) noexcept {

    PacketHeader header(PacketKind::KERNEL_DISPATCH, scope, BarrierBit::ENABLE);
    Base::addTaskToQ(mDispQueue, ki, header, compSig);
  }

  TaskHandle launchTask(const GpuKernInstance& ki) noexcept {
    Signal compSig = execResource().signalPool().takeUserSignal();
    addTaskImpl(ki, compSig, FenceScope::SYSTEM);
    // addTaskImpl(ki, compSig, FenceScope::AGENT);
    mDispQueue.submitPackets();

    return TaskHandle {compSig};
  }

  void launchAndForget(const GpuKernInstance& ki) noexcept {
    addTaskImpl(ki, impl::NULL_SIGNAL, FenceScope::SYSTEM);
    mDispQueue.submitPackets();
  }

  struct BatchState {
    Signal mFinalSig;

    explicit BatchState(SerialOrderedExecutor& exec) noexcept:
      mFinalSig(exec.execResource().signalPool().takeUserSignal())
    {}

  };

  BatchState startBatch() noexcept {
    return BatchState(*this);
  }

  void addToBatch(BatchState& batchState, const GpuKernInstance& ki) noexcept {
    addTaskImpl(ki, impl::NULL_SIGNAL, FenceScope::AGENT);
  }

  TaskHandle finishBatch(BatchState& batchState, const GpuKernInstance& ki) noexcept {
    addTaskImpl(ki, batchState.mFinalSig, FenceScope::SYSTEM);

    mDispQueue.submitPackets();

    return TaskHandle {batchState.mFinalSig};
  }


};


class SerialUnorderedExecutor: public SerialGpuExecutorBase {

  using Base = SerialGpuExecutorBase;

  using QueuesVec = std::vector<DispatchQueueSerial>;

  constexpr static const size_t MAX_ACTIVE_QUEUES = 64ul;

  size_t mNumQueues;
  QueuesVec mQueues;
  size_t mCurrQid = 0ul;

  size_t nextQid() noexcept {
    do {
      mCurrQid = (mCurrQid + 1) % mNumQueues;
    } while (mQueues[mCurrQid].full());

    assert(mCurrQid < mQueues.size());
    return mCurrQid;
  }

  void addTaskImpl(size_t qid, const GpuKernInstance& ki, const Signal& compSig, const FenceScope& scope) noexcept {

    PacketHeader header(PacketKind::KERNEL_DISPATCH, scope, BarrierBit::DISABLE);
    Base::addTaskToQ(mQueues[qid], ki, header, compSig);
  }

public:

  SerialUnorderedExecutor(GpuExecutionResource* e, size_t numQs) noexcept:
    Base(e),
    mNumQueues(std::max(1ul, numQs)),
    mQueues() 
  {
    mNumQueues = std::min(numQs, MAX_ACTIVE_QUEUES);

    for (size_t i = 0; i < mNumQueues; ++i) {
      mQueues.emplace_back(execResource().queuePool().takeSerialQueue());
    }
  }

  ~SerialUnorderedExecutor() noexcept {
    for (auto& q: mQueues) {
      execResource().queuePool().returnSerialQueue(q.hsaQueue());
    }
  }

  TaskHandle launchTask(const GpuKernInstance& ki) noexcept {
    auto qid = nextQid();
    Signal compSig = execResource().signalPool().takeUserSignal();
    addTaskImpl(qid, ki, compSig, FenceScope::SYSTEM);
    mQueues[qid].submitPackets();
    return TaskHandle {compSig};
  }

  void launchAndForget(const GpuKernInstance& ki) noexcept {
    auto qid = nextQid();
    addTaskImpl(qid, ki, impl::NULL_SIGNAL, FenceScope::SYSTEM);
    mQueues[qid].submitPackets();
  }

  struct BatchState {
    using SignalVec = std::vector<Signal>;

    SignalVec mPerQcompSig;
    Signal mFinalSig;

    explicit BatchState(SerialUnorderedExecutor& exec) noexcept:
      mPerQcompSig(),
      mFinalSig()
    {
      for (size_t i = 0; i < exec.mNumQueues; ++i) {
        mPerQcompSig.emplace_back(exec.execResource().signalPool().takeUserSignal());
      }

      assert(mPerQcompSig.size() == exec.mNumQueues);
      mFinalSig = exec.execResource().signalPool().takeUserSignal();

      for (auto& sig: mPerQcompSig) {
        hsa_signal_store_relaxed(sig, 0u);
      }
    }

    // TODO(amber): add a destructor that returns signals to pool
    // TODO(amber): make class moveable only (in order to achieve the above
    // correctly)

  };

  BatchState startBatch() {
    return BatchState(*this);
  }

  /**
   * Start a dependent batch by 
   * inserting a barrier pkt dependent on depSig 
   * in all queues
   */
  BatchState startBatchWithDep(const Signal& depSig) {

    Signal depSigArr[] = { depSig };
    PacketHeader header(PacketKind::BARRIER_AND, FenceScope::AGENT, BarrierBit::ENABLE);

    for (auto& q: mQueues) {
      AqlPacket* pkt = q.giveOneSlot();
      PacketFactory::init(reinterpret_cast<BarrierAndPkt*>(pkt), header, impl::NULL_SIGNAL, depSigArr, 1u);
      
    }

    return startBatch();
  }

  void addToBatch(BatchState& batchState, const GpuKernInstance& ki) noexcept {
    size_t qid = nextQid();
    hsa_signal_add_relaxed(batchState.mPerQcompSig[qid], 1);
    addTaskImpl(qid, ki, batchState.mPerQcompSig[qid], FenceScope::AGENT);
  }

  TaskHandle launchBatch(BatchState& batchState) noexcept {
    size_t numBarrierPkts = PacketFactory::barrierTreeSize(mNumQueues);
    assert(numBarrierPkts == 1);

    // std::printf("Adding the last barrier pkt\n");
   
    AqlPacket* pkt = mQueues[0].giveOneSlot();
    PacketHeader header(PacketKind::BARRIER_AND, FenceScope::SYSTEM, BarrierBit::ENABLE);
    PacketFactory::init(reinterpret_cast<BarrierAndPkt*>(pkt), header, batchState.mFinalSig, batchState.mPerQcompSig, mNumQueues);
    
    for (auto& q: mQueues) {
      q.submitPackets();
    }

    return TaskHandle {batchState.mFinalSig};
  }

  TaskHandle finishBatch(BatchState& batchState, const GpuKernInstance& ki) noexcept {
    addToBatch(batchState, ki);
    return launchBatch(batchState);
  }

};
constexpr size_t SerialUnorderedExecutor::MAX_ACTIVE_QUEUES;

}// end namespace dagr


#endif// DAGEE_INCLUDE_DAGR__EXECUTOR_H_
