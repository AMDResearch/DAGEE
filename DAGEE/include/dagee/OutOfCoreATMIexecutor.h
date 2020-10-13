// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_OUT_OF_CORE_ATMI_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_OUT_OF_CORE_ATMI_EXECUTOR_H

#include "dagee/ATMIdagExecutor.h"
#include "dagee/AllocFactory.h"
#include "dagee/NullKernel.h"
#include "dagee/Partition.h"

#include "atmi.h"
#include "atmi_runtime.h"

namespace dagee {

template <typename Exec, typename AllocFactory = dagee::StdAllocatorFactory<>>
class OutOfCoreATMIexecutor : private ATMIdagExecutor<Exec, AllocFactory> {
  using Base = ATMIdagExecutor<Exec, AllocFactory>;
  using typename Base::ATMIhandleVec;
  using typename Base::DAGptr;
  using KernelInfo = typename Exec::KernelInfo;
  using typename Base::TaskInstance;
  using typename Base::NodePtr;

  Exec& mExec;

 public:
  // using AsyncMode = ATMIdagExecutor<AllocFactory, ATMI_FALSE>;
  // using SyncMode = ATMIdagExecutor<AllocFactory, ATMI_TRUE>;
  // template <typename AF>
  // using withAllocFactory = ATMIdagExecutor<AF, SYNCHRONOUS>;

  using Partition = dagee::Partition<DAGptr, AllocFactory>;
  using HostData = typename Partition::HostData;
  using DeviceData = typename Partition::DeviceData;
  using PartitionDAG = typename DAGbase<Partition, AllocFactory>::WithPredSucc;
  using PartitionDAGptr = PartitionDAG*;
  using PartNode = typename PartitionDAG::Node;
  using PartNodePtr = typename PartitionDAG::NodePtr;

 protected:
  using PartDAGmgr = DAGmanager<PartitionDAG, AllocFactory>;

  using VecPartDevData = typename AllocFactory::template Vec<DeviceData*>;

  KernelInfo mNullKern;
  PartDAGmgr mPartDAGmgr;
  VecPartDevData mDeviceDataSlots;

  void assignDeviceDataSlots(PartitionDAGptr partDAG) {
    const size_t N = mDeviceDataSlots.size();

    typename AllocFactory::template Vec<PartNodePtr> prevUser(
        N, nullptr);  // of a device data slot

    using Pair = std::pair<PartNodePtr, PartNodePtr>;
    typename AllocFactory::template Vec<Pair> newEdges;

    size_t index = 0;  // round-robbins over mDeviceDataSlots and prevUser
    partDAG->forEachNode_TopoOrder([&, this](PartNodePtr partNode) {
      auto& part = partDAG->nodeData(partNode);

      part.deviceData() = mDeviceDataSlots[index];
      if (prevUser[index]) {
        newEdges.emplace_back(prevUser[index], partNode);
      }
      prevUser[index] = partNode;

      index = (index + 1) % N;
    });

    for (auto& p : newEdges) {
      partDAG->addEdgeIfAbsent(p.first, p.second);
    }
  }

  void makePerPartitionDAGs(PartitionDAGptr partDAG) {
    partDAG->forEachNode([&, this](PartNodePtr partNode) {
      auto& part = partDAG->nodeData(partNode);
      part.fillDAG(this->makeDAG());
    });
  }

  void preProcess(PartitionDAGptr partDAG) {
    assignDeviceDataSlots(partDAG);
    makePerPartitionDAGs(partDAG);
  }

  template <typename V>
  void collectPredHandles(PartitionDAGptr partDAG,
                          PartNodePtr node,
                          V& predHandles) {
    for (PartNodePtr n : partDAG->predecessors(node)) {
      predHandles.emplace_back(partDAG->nodeData(n).sinkTask());
    }
  }

  template <typename V>
  ATMItaskHandle makeNullTask(const V& predHandles) {
    auto ki = mExec.makeTask(dim3(1), dim3(1), mNullKern, nullptr);
    return impl::makeInternalTaskForDag(&mExec, ki, predHandles);
  }

  template <typename PartDataT1, typename PartDataT2>
  ATMItaskHandle makeMemCpyTasks(PartDataT1* dst,
                                 PartDataT2* src,
                                 const ATMItaskHandle& predTask) {
    assert(dst && "dst ptr null");
    assert(src && "src ptr null");
    assert(src != dst && "source and destination of memcpy can't be same");
    assert(src->numBlocks() == dst->numBlocks() &&
           "src and dst must have equal numBlocks");

    ATMIhandleVec preds{predTask};

    ATMIhandleVec memTasks;

    for (auto i = src->begin(), end_i = src->end(); i != end_i; ++i) {
      ATMI_CPARM(cparm);
      cparm->requires = &preds[0];
      cparm->num_required = preds.size();

      const auto& varName = src->varName(i);

      ATMItaskHandle th = atmi_memcpy_async(
          cparm, dst->template blkPtr<void>(varName),
          PartDataT2::template blkPtr<void>(i), PartDataT2::blkSz(i));

      memTasks.emplace_back(th);
    }

    auto sink = makeNullTask(memTasks);
    return sink;
  }

  ATMItaskHandle makeDAGtasks(DAGptr dag, const ATMItaskHandle& memCpySink) {
    ATMIhandleVec predHandles;
    ATMIhandleVec sinkHandles;

    dag->forEachNode_TopoOrder([&, this](NodePtr task) {
      predHandles.clear();

      if (dag->isSrc(task)) {
        predHandles.emplace_back(memCpySink);
      } else {
        for (const auto* p : dag->predecessors(task)) {
          const auto& pdata = dag->nodeData(p);
          predHandles.emplace_back(pdata.mATMItaskHandle);
        }
      }

      auto& tdata = dag->nodeData(task);
      tdata.mATMItaskHandle = impl::makeInternalTaskForDag(&mExec, tdata, predHandles);

      // collect sinks
      if (dag->isSink(task)) {
        sinkHandles.emplace_back(tdata.mATMItaskHandle);
      }
    });

    auto finalSink = makeNullTask(sinkHandles);
    return finalSink;
  }

  template <typename V1, typename V2>
  void makeTasksForPartition(PartitionDAGptr partDAG,
                             PartNodePtr pNode,
                             V1& allSources,
                             V2& allSinks) {
    const bool isSrc = partDAG->isSrc(pNode);
    auto& part = partDAG->nodeData(pNode);

    ATMIhandleVec preds;
    if (!isSrc) {
      collectPredHandles(partDAG, pNode, preds);
    }
    part.srcTask() = makeNullTask(preds);

    if (isSrc) {
      allSources.emplace_back(part.srcTask());
    }

    auto memCpySink =
        makeMemCpyTasks(part.deviceData(), part.hostData(), part.srcTask());

    auto dagSink = makeDAGtasks(part.dag(), memCpySink);

    part.sinkTask() =
        makeMemCpyTasks(part.hostData(), part.deviceData(), dagSink);

    if (partDAG->isSink(pNode)) {
      allSinks.emplace_back(part.sinkTask());
    }
  }

  void freeDeviceData(void) {
    for (auto* p : mDeviceDataSlots) {
      delete p;
    }
    mDeviceDataSlots.clear();
  }

 public:
  explicit OutOfCoreATMIexecutor(Exec& exec) : Base(exec), mExec(exec) {

    mNullKern = mExec.Exec::template registerKernel<int*>(&dagee::nullKernel<>);
  };

  ~OutOfCoreATMIexecutor(void) { freeDeviceData(); }

  PartitionDAGptr makePartitionDAG(void) { return mPartDAGmgr.makeDAG(); }

  void makeDeviceDataBuffers(const HostData& dataTemplate, size_t numBuffers) {
    freeDeviceData();

    for (size_t i = 0; i < numBuffers; ++i) {
      mDeviceDataSlots.emplace_back(new DeviceData(dataTemplate));
    }
  }

  size_t numDeviceDataSlots(void) const { return mDeviceDataSlots.size(); }

  void preparePartitionDAG(PartitionDAGptr partDAG) { preProcess(partDAG); }

  void executePartitionDAG(PartitionDAGptr partDAG) {
    ATMIhandleVec sources;
    ATMIhandleVec sinks;

    partDAG->forEachNode([&, this](PartNodePtr p) {
      makeTasksForPartition(partDAG, p, sources, sinks);
    });

    impl::activateTasks(sources.begin(), sources.end());

    impl::waitOnTasks(sinks.begin(), sinks.end());
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<>>
class OutOfCoreSynchronousExecutor : public OutOfCoreATMIexecutor<AllocFactory> {
  using Base = OutOfCoreATMIexecutor<AllocFactory>;

 public:
  using typename Base::Partition;
  using typename Base::PartitionDAGptr;
  using typename Base::PartNodePtr;

 protected:
  template <typename PartDataT1, typename PartDataT2, typename CopyFunc>
  void syncMemCpy(PartDataT1* dst, PartDataT2* src, const CopyFunc& copyFunc) {
    for (auto i = src->begin(), end_i = src->end(); i != end_i; ++i) {
      const auto& varName = src->varName(i);

      copyFunc(dst->template blkPtr<uint8_t>(varName),
               PartDataT2::template blkPtr<uint8_t>(i), PartDataT2::blkSz(i));
    }
  }

  void copyIn(Partition& part) {
    syncMemCpy(part.deviceData(), part.hostData(), &dagee::copyToDevice<uint8_t>);
  }

  void copyOut(Partition& part) {
    syncMemCpy(part.hostData(), part.deviceData(), &dagee::copyToHost<uint8_t>);
  }

 public:
  void executePartitionDAG(PartitionDAGptr partDAG) {
    partDAG->forEachNode_TopoOrder([&, this](PartNodePtr partNode) {
      auto& part = partDAG->nodeData(partNode);
      copyIn(part);
      Base::executeParallel({part.dag()});
      copyOut(part);
    });
  }
};

}  // end namespace dagee

#endif  // DAGEE_INCLUDE_DAGEE_OUT_OF_CORE_ATMI_EXECUTOR_H
