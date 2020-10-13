// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_DAG_GAUGE_H
#define DAGEE_INCLUDE_DAGEE_DAG_GAUGE_H

#include "dagee/ATMIdagExecutor.h"

#include <fstream>
#include <iostream>

namespace dagee {


template <typename Exec, typename AllocFactory = dagee::StdAllocatorFactory<> >
class DAGparallelismExecutor: public ATMIdagExecutor<Exec, AllocFactory> {
  using Base = ATMIdagExecutor<Exec, AllocFactory>;


public:
  using typename Base::TaskInstance;
  using DAG = typename DAGbase<TaskInstance, AllocFactory>::WithPredSucc;
  using DAGptr = DAG*;
  using Task = typename DAG::Task;
  using TaskPtr = typename DAG::TaskPtr;

protected:

  using DAGmgr = DAGmanager<DAG, AllocFactory>;
  using TaskPtrVec = typename AllocFactory::template Vec<TaskPtr>;
  using TaskQ = typename AllocFactory::template Deque<TaskPtr>;
  using ATMIhandleVec = typename AllocFactory::template Vec<ATMItaskHandle>;

  struct DAGworklists {
    DAGptr mDAG;
    TaskPtrVec _mV0;
    TaskPtrVec _mV1;
    TaskPtrVec* mCurr = &_mV0;
    TaskPtrVec* mNext = &_mV1;

    explicit DAGworklists(DAGptr dag): mDAG(dag) {}
  };

  using VecDAGworklists = typename AllocFactory::template Vec<DAGworklists>;


  static constexpr unsigned WI_PER_WF = 64;
  const char* const DELIM = ", ";

  std::ofstream mOutFile;
  DAGmgr mDAGmgr;


  template <typename V>
  void writeStep(size_t step, const V& vecDAGworklists) {

    size_t numTasks = 0;

    size_t numWI = 0;
    size_t numWF = 0;

    // TODO: add avg in-degree and avg out-degree

    for (const auto& sd: vecDAGworklists) {

      numTasks += sd.mCurr->size();

      for (auto src: *sd.mCurr) {
        const auto& ki = sd.mDAG->getTaskData(src);

        numWI += (ki.mThreadsPerBlock.x * ki.mThreadsPerBlock.y * ki.mThreadsPerBlock.z)
          * (ki.mBlocks.x * ki.mBlocks.y * ki.mBlocks.z);

      }
    }
    numWF = (numWI  + WI_PER_WF - 1)/ WI_PER_WF; // Round up

    mOutFile << step << DELIM << numTasks << DELIM << numWF << DELIM << numWI << std::endl;

  }


public:
  explicit DAGparallelismExecutor(void):
    mOutFile("parallelismProfile.csv")
  {
    mOutFile << "Step" << DELIM << "numTasks" << DELIM << "numWF" << DELIM << "numWI" << std::endl;
  }

  DAGptr makeDAG(void) { return mDAGmgr.makeDAG(); }

  void destroyDAG(DAGptr d) { mDAGmgr.destroyDAG(d); }

  template <typename DAGptrIter>
  void executeParallel(DAGptrIter beg, DAGptrIter end) {

    VecDAGworklists vecDAGworklists;
    vecDAGworklists.reserve(std::distance(beg, end));


    for (auto i = beg; i != end; ++i) {
      vecDAGworklists.emplace_back(*i);
      auto& sd = vecDAGworklists.back();
      sd.mDAG->collectInitialSources(*sd.mNext);
    }

    size_t step = 0;

    /*
     * Every iteration, for each DAG
     *    we execute current sources (and collect stats about them
     *    via writeStep().
     *    Then we collect next set of sources in a new worklist. Swap worklists and
     * repeat
     */
    while (true) {

      bool done = true;

      for (auto& sd: vecDAGworklists) {
        std::swap(sd.mNext, sd.mCurr);
        sd.mNext->clear();

        done = done && sd.mCurr->empty();
      }

      if (done) { break; }

      writeStep(step, vecDAGworklists);
      ++step;

      for (auto& sd: vecDAGworklists) {
        for (TaskPtr src: *sd.mCurr) {
          const auto& kinst = sd.mDAG->getTaskData(src);
          Base::launchKernel(kinst);

          sd.mDAG->findNextSources(src, *sd.mNext);
        }
      }

    }

  }

  void executeParallel(std::initializer_list<DAGptr> dagArr) {
    executeParallel(dagArr.begin(), dagArr.end());
  }

  void execute(DAGptr dag) {
    executeParallel({dag});
  }

};




}

#endif// DAGEE_INCLUDE_DAGEE_DAG_GAUGE_H
