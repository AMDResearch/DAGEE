// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_ATMI_MIXED_DAG_EXECUTOR_H
#define DAGEE_INCLUDE_DAGEE_ATMI_MIXED_DAG_EXECUTOR_H

#include "dagee/ATMIcpuExecutor.h"
#include "dagee/ATMIgpuExecutor.h"
#include "dagee/ATMImemCopyExecutor.h"
#include "dagee/AllocFactory.h"
#include "dagee/TaskDAG.h"

#include "cpputils/Timer.h"

#include <vector>

namespace dagee {


template <typename AllocFactory=dagee::StdAllocatorFactory<>, typename... Execs>
struct AtmiMixedDagExecutor {

  template <typename AF>
  using WithAllocFactory = AtmiMixedDagExecutor<AF, Execs...>;

  using DAG = typename MixedTaskDag<AllocFactory, false, true, typename Execs::TaskInstance...>::WithPredSucc;
  using DAGptr = DAG*;
  using Node = typename DAG::Node;
  using NodePtr = typename DAG::NodePtr;
  using NodeCptr = typename DAG::NodeCptr;

private:
  using DAGmgr = DAGmanager<DAG, AllocFactory>;
  using NodePtrVec = typename AllocFactory::template Vec<NodePtr>;
  using NodeQ = typename AllocFactory::template Deque<NodePtr>;
  using ATMIhandleVec = typename AllocFactory::template Vec<ATMItaskHandle>;

  using ExecTuple = std::tuple<typename std::add_pointer<Execs>::type...>;
  using TaskInstTuple = std::tuple<typename Execs::TaskInstance...>;


  DAGmgr mDAGmgr;
  ExecTuple mExecutors;

  struct CollectPreds {
    ATMIhandleVec& predHandles;

    template <typename D>
    void operator () (const D& nodeData) {
      predHandles.emplace_back(nodeData.mATMItaskHandle); // TODO: switch to a function
    }
  };

  struct CreateTask {
    AtmiMixedDagExecutor& outer;
    ATMIhandleVec& predHandles;
    ATMItaskHandle& th;

    template <typename D>
    void operator () (D& nodeData) {
      constexpr size_t ID = impl::getTypeId<D>(static_cast<TaskInstTuple*>(nullptr));
      th = impl::makeInternalTaskForDag(std::get<ID>(outer.mExecutors), nodeData, predHandles);
      nodeData.mATMItaskHandle = th;
    }
  };

  template <typename V1, typename V2>
  void makeLazyTasks(DAGptr dag, V1& srcHandles, V2& sinkHandles) {
    ATMIhandleVec predHandles;

    dag->forEachNode_TopoOrder(
        [&, this] (NodePtr task) {
          predHandles.clear();

          for (const auto* p: dag->predecessors(task)) {
            dag->applyToNodeData(p, CollectPreds {predHandles});
          }

          ATMItaskHandle th;
          CreateTask createTaskFn{*this, predHandles, th};
          dag->applyToNodeData(task, createTaskFn);


          if (dag->isSrc(task)) {
            srcHandles.emplace_back(th);
          }

          if (dag->isSink(task)) {
            sinkHandles.emplace_back(th);
          }
        });
  }

  template <typename I>
  inline void activateSources( const I& beg, const I& end) {
    impl::activateTasks(beg, end);
  }

  template <typename I>
  inline void waitForSinks(const I& beg, const I& end) {
    impl::waitOnTasks(beg, end);
  }



public:

  AtmiMixedDagExecutor(Execs&... executors):
    mExecutors(std::make_tuple(&executors...))
  {}

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

    activateSources(srcHandles.begin(), srcHandles.end());

    t0.stop();

    cpputils::Timer t1("HIP-ATMI", "ATMI-DAG-Execute", true);

    waitForSinks(sinkHandles.begin(), sinkHandles.end());

    t1.stop();
  }

  void executeParallel(std::initializer_list<DAGptr> dagArr) {
    executeParallel(dagArr.begin(), dagArr.end());
  }

  void execute(DAGptr dag) {
    executeParallel({dag});
  }


};

template <typename... Execs, typename AF=dagee::StdAllocatorFactory<> >
auto makeMixedDagExecutor(Execs&... executors)
  -> AtmiMixedDagExecutor<AF, Execs...> {

    return AtmiMixedDagExecutor<AF, Execs...>(executors...);
}


}// end namespace dagee



#endif// DAGEE_INCLUDE_DAGEE_ATMI_MIXED_DAG_EXECUTOR_H
