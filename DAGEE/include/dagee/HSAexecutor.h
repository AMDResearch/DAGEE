// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_HSA_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_HSA_EXECUTOR_H

#include "dagee/DeviceDAG.h"
#include "dagee/HIPinternal.h"
#include "dagee/Util.h"

#include "hip/hip_runtime.h"
#include "hip/hcc_detail/program_state.hpp"

#include "hsa/hsa.h"

#include <iostream>
#include <vector>
namespace dagee {

namespace impl { 

/*
 * Determines if the given agent is of type HSA_DEVICE_TYPE_GPU
 * and sets the value of data to the agent handle if it is.
 */
hsa_status_t get_gpu_agent(hsa_agent_t agent, void *data) {
    hsa_status_t status;
    hsa_device_type_t device_type;
    status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == device_type) {
        hsa_agent_t* ret = (hsa_agent_t*)data;
        *ret = agent;
        return HSA_STATUS_INFO_BREAK;
    }
    return HSA_STATUS_SUCCESS;
}

/*
 * Determines if a memory region can be used for kernarg
 * allocations.
 */
hsa_status_t get_kernarg_memory_region(hsa_region_t region, void* data) {
    hsa_region_segment_t segment;
    hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment);
    if (HSA_REGION_SEGMENT_GLOBAL != segment) {
        return HSA_STATUS_SUCCESS;
    }

    hsa_region_global_flag_t flags;
    hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG) {
        hsa_region_t* ret = (hsa_region_t*) data;
        *ret = region;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

uint16_t hsaPacketHeader(hsa_packet_type_t type) {

  uint16_t header = type << HSA_PACKET_HEADER_TYPE;

  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE;

  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE;

  return header;

}

uint16_t kernel_dispatch_setup() {
  return 3 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;

}

void packet_store_release(uint32_t* packet, uint16_t header, uint16_t rest) {

    __atomic_store_n(packet, header | (rest << 16),   __ATOMIC_RELEASE);

}

void initDispatchPacket(hsa_kernel_dispatch_packet_t* packet, const KernelInstance& ki, void* argPtr) {

  // Reserved fields, private and group memory, and completion signal are all set to 0.

  std::memset(((uint8_t*) packet) + 4, 0, sizeof(hsa_kernel_dispatch_packet_t) - 4);


  packet->workgroup_size_x = ki.mThreadsPerBlock.x;
  packet->workgroup_size_y = ki.mThreadsPerBlock.y;
  packet->workgroup_size_z = ki.mThreadsPerBlock.z;

  packet->grid_size_x = ki.mBlocks.x * ki.mThreadsPerBlock.x;
  packet->grid_size_y = ki.mBlocks.y * ki.mThreadsPerBlock.y;
  packet->grid_size_z = ki.mBlocks.z * ki.mThreadsPerBlock.z;


  // Indicate which executable code to run.

  // The application is expected to have finalized a kernel (for example, using the finalization API).

  // We will assume that the kernel object containing the executable code is stored in KERNEL_OBJECT

  packet->kernel_object = ki.mKernInfo.codeObjAddr();

  CHECK_HSA(hsa_signal_create(1, 0, NULL, &(packet->completion_signal)));

  // Assume our kernel receives no arguments

  packet->kernarg_address = NULL;

  packet->kernarg_address = argPtr;
  packet->private_segment_size = ki.mKernInfo.privateSegmentSize();
  packet->group_segment_size = ki.mKernInfo.groupSegmentSize();

  packet_store_release( reinterpret_cast<uint32_t*> (packet), 
      hsaPacketHeader(HSA_PACKET_TYPE_KERNEL_DISPATCH), kernel_dispatch_setup());

}

void destructDispatchPacket(hsa_kernel_dispatch_packet_t* packet) {
    CHECK_HSA(hsa_signal_destroy(packet->completion_signal));
}

void initBarrierPacket(hsa_barrier_and_packet_t* packet) {
  memset(((uint8_t*) packet) + 4, 0, sizeof(*packet) - 4);
  packet_store_release((uint32_t*) packet, hsaPacketHeader(HSA_PACKET_TYPE_BARRIER_AND), 0);
}


} // end namespace impl

/**
 * Dispatches one kernel instance at a time
 */
class SerialHSAexecutor {

  hsa_agent_t mAgent;
  hsa_queue_t* mQueue;
  hsa_region_t mKernArgRegion;
  uint32_t mKernArgSegSize = 0u;
  uint8_t* mKernArgHead = nullptr;
  uint8_t* mKernArgTail = nullptr;

  void init(void) {

    hsa_init();

    hsa_agent_t agent;
    hsa_status_t err = hsa_iterate_agents(&impl::get_gpu_agent, &agent);
    if (err == HSA_STATUS_INFO_BREAK) {
      err = HSA_STATUS_SUCCESS;
    } else {
      err = HSA_STATUS_ERROR;
    }
    CHECK_HSA(err);

    /*
     * Query the name of the agent.
     */
    // char name[64] = { 0 };
    // CHECK_HSA(hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name));
    // std::cout << "GPU agent name: " << name << std::endl;

    /*
     * Query the maximum size of the queue.
     */
    uint32_t queue_size = 0;
    CHECK_HSA(hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size));
    // std::cout << "The maximum queue size is " << queue_size << std::endl;
    /**
    * Create a queue using the maximum size.
    */
    // TODO: limit queue size
    hsa_queue_t* queue; 
    CHECK_HSA(hsa_queue_create(agent, queue_size, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue));

    /**
     * locate kernarg_region
     */

    hsa_region_t kernarg_region;
    kernarg_region.handle=(uint64_t)-1;
    hsa_agent_iterate_regions(agent, &impl::get_kernarg_memory_region, &kernarg_region);
    err = (kernarg_region.handle == (uint64_t)-1) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
    CHECK_HSA(err);

    /**
     * kernarg segment size
     */

    uint32_t kernarg_segment_size = 0;
    CHECK_HSA(hsa_region_get_info(kernarg_region, HSA_REGION_INFO_SIZE, &kernarg_segment_size));
    // std::cout << "kernarg_segment_size is: " << kernarg_segment_size << std::endl;

    /**
     * Allocate the kernel argument buffer from the correct region.
     */   
    void* kernarg_address = NULL;
    CHECK_HSA(hsa_memory_allocate(kernarg_region, kernarg_segment_size, &kernarg_address));

    
    this->mAgent = agent;
    this->mQueue = queue;

    this->mKernArgRegion = kernarg_region;
    this->mKernArgSegSize = kernarg_segment_size;
    this->mKernArgHead = reinterpret_cast<uint8_t*>(kernarg_address);
    this->mKernArgTail = reinterpret_cast<uint8_t*>(kernarg_address);
  }

  void shutdown(void) {
    CHECK_HSA(hsa_memory_free(mKernArgHead));
    CHECK_HSA(hsa_queue_destroy(mQueue));
    CHECK_HSA(hsa_shut_down());
  }

  void* initArgs(const KernelInstance& ki) {
    memcpy(this->mKernArgTail, ki.mKernArgs.data(), ki.mKernArgs.size());
    auto* argPtr = mKernArgTail;
    this->mKernArgTail += ki.mKernArgs.size();

    return reinterpret_cast<void*> (argPtr);
  }

  void freeArgs(const KernelInstance& ki) {
    this->mKernArgTail -= ki.mKernArgs.size();
  }

  void initDispatchPacket(hsa_kernel_dispatch_packet_t* packet, const KernelInstance& ki) {
    void* argPtr = initArgs(ki);
    impl::initDispatchPacket(packet, ki, argPtr);
  }

  void destructDispatchPacket(hsa_kernel_dispatch_packet_t* packet, const KernelInstance& ki) {
    impl::destructDispatchPacket(packet);
    freeArgs(ki);
  }

  template <typename P>
  P* getPacketAddr(uint64_t index) {
    const uint64_t qMask = mQueue->size - 1;
    P* base = reinterpret_cast<P*>(mQueue->base_address);
    return &base[index & qMask];
  }

public:
  SerialHSAexecutor(void) {
    init();
  }

  ~SerialHSAexecutor(void) {
    shutdown();
  }

  void dispatch(const KernelInstance& ki) {
    /*
     * Obtain the current queue write index.
     */
    uint64_t index = hsa_queue_load_write_index_relaxed(mQueue);

    /*
     * Write the aql packet at the calculated queue index address.
     */
    hsa_kernel_dispatch_packet_t* dispatch_packet = getPacketAddr<hsa_kernel_dispatch_packet_t>(index);

    this->initDispatchPacket(dispatch_packet, ki);

    /*
     * Increment the write index and ring the doorbell to dispatch the kernel.
     */
    hsa_queue_store_write_index_relaxed(mQueue, index+1);
    hsa_signal_store_relaxed(mQueue->doorbell_signal, index);
    std::cout << "Dispatching the kernel" << std::endl;

    /*
     * Wait on the dispatch completion signal until the kernel is finished.
     */
    hsa_signal_value_t value = hsa_signal_wait_acquire(dispatch_packet->completion_signal, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);

    destructDispatchPacket(dispatch_packet, ki);
  }

  template <typename G>
  void executeDAG(G& dag) {
    using Task = typename G::Task;

    std::deque<Task*> sources;
    dag.collectInitialSources(sources);

    while(!sources.empty()) {
      Task* t = sources.front();
      assert(t && t->isSrc() && "Invalid task removed from queue");
      sources.pop_front();

      dispatch(t->getFunc());

      dag.markExecuted(t, sources);
    }
  }


  void dispatchKiteDAG(const KernelInstance& top, const KernelInstance& left, const KernelInstance& right, const KernelInstance& bottom) {

    uint64_t index = hsa_queue_load_write_index_relaxed(mQueue);

    const uint64_t qMask = mQueue->size - 1;

    assert(mQueue->size > 6);

    hsa_kernel_dispatch_packet_t* topPkt = getPacketAddr<hsa_kernel_dispatch_packet_t>(index);

    hsa_barrier_and_packet_t* topBarrierPkt = getPacketAddr<hsa_barrier_and_packet_t>(index + 1);

    hsa_kernel_dispatch_packet_t* leftPkt = getPacketAddr<hsa_kernel_dispatch_packet_t>(index + 2);

    hsa_kernel_dispatch_packet_t* rightPkt = getPacketAddr<hsa_kernel_dispatch_packet_t>(index + 3);

    hsa_barrier_and_packet_t* bottomBarrierPkt = getPacketAddr<hsa_barrier_and_packet_t>(index + 4);

    hsa_kernel_dispatch_packet_t* bottomPkt = getPacketAddr<hsa_kernel_dispatch_packet_t>(index + 5);

    this->initDispatchPacket(topPkt, top);
    this->initDispatchPacket(leftPkt, left);
    this->initDispatchPacket(rightPkt, right);
    this->initDispatchPacket(bottomPkt, bottom);

    impl::initBarrierPacket(topBarrierPkt);
    impl::initBarrierPacket(bottomBarrierPkt);

    topBarrierPkt->dep_signal[0] = topPkt->completion_signal;
    bottomBarrierPkt->dep_signal[0] = leftPkt->completion_signal;
    bottomBarrierPkt->dep_signal[1] = rightPkt->completion_signal;

    std::cout << "Dispatching  kite DAG" << std::endl;
    hsa_queue_add_write_index_screlease(mQueue, 6);
    hsa_signal_store_relaxed(mQueue->doorbell_signal, index);
    hsa_signal_store_relaxed(mQueue->doorbell_signal, index + 2);
    hsa_signal_store_relaxed(mQueue->doorbell_signal, index + 3);
    hsa_signal_store_relaxed(mQueue->doorbell_signal, index + 5);

    hsa_signal_value_t value = hsa_signal_wait_acquire(bottomPkt->completion_signal, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);

    this->destructDispatchPacket(bottomPkt, bottom);
    this->destructDispatchPacket(rightPkt, right);
    this->destructDispatchPacket(leftPkt, left);
    this->destructDispatchPacket(topPkt, top);
  }

};


} // end namespace dagee

#endif// DAGEE_INCLUDE_DAGEE_HSA_EXECUTOR_H
