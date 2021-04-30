// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__QUEUE_H_
#define DAGEE_INCLUDE_DAGR__QUEUE_H_

#include "dagr/hsaCore.h"

#include "cpputils/Container.h"

#include <vector>
#include <utility>

namespace dagr {

enum class HsaQueueKind: std::underlying_type<hsa_queue_type_t>::type {
  SINGLE_PRODUCER = HSA_QUEUE_TYPE_SINGLE,
  MULTI_PRODUCER = HSA_QUEUE_TYPE_MULTI,
  COOPERATIVE = HSA_QUEUE_TYPE_COOPERATIVE
};

struct HsaQueueFactoryBase {
};

template <HsaQueueKind KIND_T>
class QueueFactoryImpl {

  using AllocatedQueues = cpputils::VectorBasedSet<HsaQueue*>;

  Agent mAgent;
  size_t mMaxQsize = 0ul;

public:

  QueueFactoryImpl(const Agent& agent) noexcept :
    mAgent(agent),
    mMaxQsize(AgentQuery::maxQueueSize(agent))
  {
    assert(mMaxQsize > 0);
  }

  HsaQueue* create() noexcept {
    HsaQueue* q = nullptr;

    // ASSERT_HSA((hsa_queue_create(mAgent, AgentQuery::minQueueSize(mAgent), static_cast<hsa_queue_type32_t>(KIND_T), nullptr, nullptr
    ASSERT_HSA((hsa_queue_create(mAgent, mMaxQsize, static_cast<hsa_queue_type32_t>(KIND_T), nullptr, nullptr
          , UINT32_MAX, UINT32_MAX, &q)));
    assert(q);
    // std::printf("HsaQueue created at %p, type=%d, kind = %d, size=%u\n", q, q->type, uint32_t(KIND_T), q->size);
    return q;
  }

  void destroy(HsaQueue* q) noexcept {
    ASSERT_HSA(hsa_queue_destroy(q));
  }
};

using SerialQueueFactory = QueueFactoryImpl<HsaQueueKind::SINGLE_PRODUCER>;
using ConcurrentQueueFactory = QueueFactoryImpl<HsaQueueKind::MULTI_PRODUCER>;

namespace impl {
  constexpr static const size_t QUEUE_POOL_BATCH_SIZE = 8ul;
}

using SerialQueuePool = 
  cpputils::ResourcePool<HsaQueue*, SerialQueueFactory, impl::QUEUE_POOL_BATCH_SIZE>;

using ConcurrentQueuePool = 
  cpputils::ResourcePool<HsaQueue*, ConcurrentQueueFactory, impl::QUEUE_POOL_BATCH_SIZE>;

// using SignalPoolIpc = SignalPoolImpl<IpcSignalFactory>;

struct HsaQueuePoolState {

  SerialQueueFactory mSerialQueueFactory;
  ConcurrentQueueFactory mConcurrentQueueFactory;
  SerialQueuePool mSerialQueuePool;
  ConcurrentQueuePool mConcurrentQueuePool;


  explicit HsaQueuePoolState(const Agent& agent) noexcept:
    mSerialQueueFactory(agent),
    mConcurrentQueueFactory(agent),
    mSerialQueuePool(mSerialQueueFactory),
    mConcurrentQueuePool(mConcurrentQueueFactory)
 {
 }


 HsaQueue* takeSerialQueue() noexcept {
   return mSerialQueuePool.allocate();
 }

 void returnSerialQueue(HsaQueue* q) noexcept {
   assert(q);
   mSerialQueuePool.deallocate(q);
 }

 HsaQueue* takeConcurrentQueue() noexcept {
   return mConcurrentQueuePool.allocate();
 }

 void returnConcurrentQueue(HsaQueue* q) noexcept {
   assert(q);
   mConcurrentQueuePool.deallocate(q);
 }

};


struct AqlPacket {
  union {
    KernelDispatchPkt mKernelDispatchPkt;
    BarrierAndPkt mBarrierAndPkt;
    BarrierOrPkt mBarrierOrPkt;
  };
};


class DispatchQueueSerial {

  HsaQueue* mHsaQueue;
  size_t mWriteIndex;
  
  size_t readIndex() const noexcept {
    // return hsa_queue_load_read_index_scacquire(mHsaQueue);
    return hsa_queue_load_read_index_relaxed(mHsaQueue);
  }

  size_t writeIndex() const noexcept {
    return mWriteIndex;
  }

  size_t sizeMinus1() const noexcept {
    return mHsaQueue->size - 1ul;
  }

public:

  size_t size() const noexcept {
    return mHsaQueue->size;
  }

  bool full() const noexcept {
    return writeIndex() >= (readIndex() + size());
  }

  bool empty() const noexcept {
    return writeIndex() == readIndex();
  }
  
private:

  size_t incWriteIndex() noexcept {
    // return (mWriteIndex = (mWriteIndex + 1) % size());
    // since size() is a power of 2, we can use bit-wise and
    // return (mWriteIndex = (mWriteIndex + N) & sizeMinus1());
    // mWriteIndex = hsa_queue_add_write_index_scacquire(mHsaQueue, N) & sizeMinus1();
    mWriteIndex = hsa_queue_add_write_index_relaxed(mHsaQueue, 1u);
    return mWriteIndex;
  }

  AqlPacket* headSlot() noexcept {
    return reinterpret_cast<AqlPacket*>(mHsaQueue->base_address) + (writeIndex() & sizeMinus1());
  }

  AqlPacket* giveOneSlotImpl() noexcept {

    while (full()) {
      // busy wait
      // std::printf("numEmptySlots() = %zu, readIndex = %zu, writeIndex = %zu\n" , numEmptySlots(), readIndex(), writeIndex());
    }

    incWriteIndex();
    AqlPacket* head = headSlot();
    // std::printf("Packet address = %p\n", head);
    return head;
  }

public:

  explicit DispatchQueueSerial(HsaQueue* q) noexcept :
    mHsaQueue(q),
    mWriteIndex(hsa_queue_load_write_index_scacquire(q))
  {
    assert(q);
  }

  AqlPacket* giveOneSlot() noexcept {
    return giveOneSlotImpl();
  }

  /*
  AqlPacket* giveSlots(const size_t N) noexcept {
    assert(N < size());
    return giveSlotsImpl(N);
  }
  */

  void submitPackets() noexcept {
    // std::printf("Ringing doorbell on index=%zu, readIndex=%zu\n", mWriteIndex, readIndex());
    hsa_signal_store_screlease(mHsaQueue->doorbell_signal, mWriteIndex);
  }

  const HsaQueue* hsaQueue() const noexcept {
    return mHsaQueue;
  }

  // FIXME: remove this non const overload
  HsaQueue* hsaQueue() noexcept {
    return mHsaQueue;
  }
};



}// end namespace dagr



#endif// DAGEE_INCLUDE_DAGR__QUEUE_H_
