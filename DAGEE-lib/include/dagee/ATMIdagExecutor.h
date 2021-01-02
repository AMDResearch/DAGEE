// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_DAG_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_DAG_EXECUTOR_H

#include "dagee/ATMIcpuExecutor.h"
#include "dagee/ATMIgpuExecutor.h"
#include "dagee/TaskDAG.h"

#include "cpputils/Timer.h"

namespace dagee {
/*
 * TODO(amber): Fix Allocators for all containers.
 * Allocator has to be default constructible for convenience, otherwise it's too
 * much work
 *
 * Secondly, allocator has to be copy constructable from allocator of other type
 *
 * We need two main allocators. Fixed size and variable size
 */
template <typename ExecT, typename AllocFactory = dagee::StdAllocatorFactory<>>
class ATMIdagExecutor {
 public:
  template <typename AF>
  using withAllocFactory = ATMIdagExecutor<ExecT, AF>;
  template <typename NE>
  using withExecutor = ATMIdagExecutor<NE, AllocFactory>;

  using TaskInstance = typename ExecT::TaskInstance;
  using DAG = typename DAGbase<TaskInstance, AllocFactory>::WithPredSucc;
  using DAGptr = DAG*;
  using Node = typename DAG::Node;
  using NodePtr = typename DAG::NodePtr;
  using NodeCptr = typename DAG::NodeCptr;

 protected:
  using DAGmgr = DAGmanager<DAG, AllocFactory>;
  using NodePtrVec = typename AllocFactory::template Vec<NodePtr>;
  using NodeQ = typename AllocFactory::template Deque<NodePtr>;
  using ATMIhandleVec = typename AllocFactory::template Vec<ATMItaskHandle>;

  DAGmgr mDAGmgr;
  ExecT& mExec;

  template <typename V1, typename V2>
  inline void makeLazyTasks(DAGptr dag, V1& srcHandles, V2& sinkHandles) {
    ATMIhandleVec predHandles;
    dag->forEachNode_TopoOrder([&, this](NodePtr task) {
      predHandles.clear();

      for (const auto* p : dag->predecessors(task)) {
        const auto& pdata = dag->nodeData(p);
        predHandles.emplace_back(pdata.mATMItaskHandle);
      }

      auto& tdata = dag->nodeData(task);
      // TODO(amber): We can't assume mATMItaskHandle field. It's better to create
      // a task data type for DAGs that composes Executor::TaskInstance and
      // mATMItaskHandle
      tdata.mATMItaskHandle = impl::makeInternalTaskForDag(&mExec, tdata, predHandles);

      // collect sources and sinks
      if (dag->isSrc(task)) {
        srcHandles.emplace_back(tdata.mATMItaskHandle);
      }

      if (dag->isSink(task)) {
        sinkHandles.emplace_back(tdata.mATMItaskHandle);
      }
    });
  }

  /*
template <typename V1, typename V2>
inline void makeLazyGpuTasks(DAGptr dag, V1& srcHandles, V2& sinkHandles) {

  NodeQ taskQ;
  NodePtrVec sources;
  NodePtrVec sinks;

  ATMIhandleVec predHandles;

  dag->collectSourcesAndSinks(sources, sinks);

  for (auto* t: sources) {
    taskQ.emplace_back(t);
  }

  while (!taskQ.empty()) {
    auto* task = taskQ.front();
    taskQ.pop_front();

    predHandles.clear();

    auto& tdata = dag->nodeData(task);
    for (const auto* p : dag->predecessors(task)) {
      const auto& pdata = dag->nodeData(p);
      predHandles.push_back(pdata.mATMItaskHandle);
    }

    auto lp = Base::initLaunchParam(tdata);

    lp.requires = &predHandles[0];
    lp.num_required = predHandles.size();

    // make the task
    tdata.mATMItaskHandle =
        atmi_task_create(&lp, tdata.mKernInfo, tdata.argAddresses().data());

    dag->findNextSources(task, taskQ);
  }

  for (NodePtr t: sources) {
    srcHandles.emplace_back(dag->nodeData(t).mATMItaskHandle);
  }

  for (NodePtr t: sinks) {
    sinkHandles.emplace_back(dag->nodeData(t).mATMItaskHandle);
  }
}
*/

 public:
  explicit ATMIdagExecutor(ExecT& exec) : mExec(exec){};

  ExecT& targetExec() noexcept { return mExec; }
  const ExecT& targetExec() const noexcept { return mExec; }

  DAGptr makeDAG(void) { return mDAGmgr.makeDAG(); }

  void destroyDAG(DAGptr d) { mDAGmgr.destroyDAG(d); }

  template <typename DAGptrIter>
  void executeParallel(DAGptrIter beg, DAGptrIter end) {
    cpputils::Timer t0("HIP-ATMI", "ATMI-DAG-Create", true);

    ATMIhandleVec srcHandles;
    ATMIhandleVec sinkHandles;

    for (auto i = beg; i != end; ++i) {
      DAGptr dag = *i;
      makeLazyTasks(dag, srcHandles, sinkHandles);
    }

    impl::activateTasks(srcHandles.begin(), srcHandles.end());

    t0.stop();

    cpputils::Timer t1("HIP-ATMI", "ATMI-DAG-Execute", true);

    impl::waitOnTasks(sinkHandles.begin(), sinkHandles.end());

    t1.stop();
  }

  void executeParallel(std::initializer_list<DAGptr> dagArr) {
    executeParallel(dagArr.begin(), dagArr.end());
  }

  void execute(DAGptr dag) { executeParallel({dag}); }

  /*
 *TODO: add static and dynamic launch mode
void execute(void) {

  std::deque<NodePtr> sources;

  std::vector<ATMItaskHandle> predHandles;
  predHandles.reserve(64);

  mDag.collectInitialSources(sources);

  ATMIgpuKernelInstance* lastTaskData = nullptr;
  while (!sources.empty()) {

    NodePtr task = sources.front();
    sources.pop_front();

    predHandles.clear();
    for (const auto* p : mDag.predecessors(task)) {

      const ATMIgpuKernelInstance& pdata = mDag.nodeData(p);
      predHandles.push_back(pdata.mATMItaskHandle);
    }

    ATMIgpuKernelInstance& tdata = mDag.nodeData(task);

    // std::cout << "Launching task: " << &tdata << std::endl;

    launchGpuTask(tdata, predHandles);
    lastTaskData = &tdata;

    mDag.findNextSources(task, sources);
  }

  if (lastTaskData) {
    // FIXME: there can be multiple sink nodes
    Base::waitOn(lastTaskData->mATMItaskHandle);
  }
}
*/
};
} // namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_DAG_EXECUTOR_H
