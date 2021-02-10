// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_SINGLE_DAG_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_SINGLE_DAG_EXECUTOR_H

#include "dagee/ATMIdagExecutor.h"
#include "dagee/AllocFactory.h"
#include "dagee/DeviceAlloc.h"
#include "dagee/DummyKernel.h"

#include "cpputils/Print.h"

#include <string>

namespace dagee {

template <typename Exec, typename AllocFactory = dagee::StdAllocatorFactory<>>
struct SingleDAGexecutor : public ATMIdagExecutor<Exec, AllocFactory>,
                           public DeviceBufferManager<AllocFactory> {
  using DagExecBase = ATMIdagExecutor<Exec, AllocFactory>;
  using DeviceAllocBase = DeviceBufferManager<AllocFactory>;

  using typename DagExecBase::DAGptr;
  using TaskPtr = typename DagExecBase::NodePtr;
  using Kernel = typename Exec::KernelInfo;

  DAGptr mDAG;
  std::string mName;

  constexpr TaskPtr nullTask(void) const { return nullptr; }

  template <typename S>
  explicit SingleDAGexecutor(Exec& exec, const S& name)
      : DagExecBase(exec), DeviceAllocBase(), mDAG(DagExecBase::makeDAG()), mName(name) {}

  ~SingleDAGexecutor(void) {
    auto p = mDAG->size();
    cpputils::printStat(mName, "Num Tasks", p.first);
    cpputils::printStat(mName, "DAG Edges", p.second);
  }

  template <typename A, typename... Args>
  TaskPtr addTask(A& preds, const dim3& blocks, const dim3& threadsPerBlock, const Kernel& kern,
                  Args... args) {
    auto t = mDAG->addNode(DagExecBase::mExec.makeTask(blocks, threadsPerBlock, kern, args...));

    for (auto p : preds) {
      if (p != nullTask()) {
        mDAG->addEdge(p, t);
      }
    }

    return t;
  }

  void execute(void) { DagExecBase::execute(mDAG); }

  void launchDummyKernel(void) const { dagee::launchDummyKernel(); }
};

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_SINGLE_DAG_EXECUTOR_H
