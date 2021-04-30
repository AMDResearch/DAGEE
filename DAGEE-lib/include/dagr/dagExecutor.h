// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGR__DAGEXECUTOR_H_
#define DAGEE_INCLUDE_DAGR__DAGEXECUTOR_H_

#include "dagr/executor.h"

#include <vector>
#include <unordered_set>

namespace dagr {

template <typename T>
struct alignas(alignof(T)) LazyObject {

  char mData[sizeof(T)] = {0};

  LazyObject& operator = (const LazyObject&) = delete;
  LazyObject& operator = (LazyObject&&) = delete;

  void init(const T& that) noexcept {
    T& myObj = get();
    new (&myObj) T(that);
  }

  T& get() noexcept {
    T* ptr = reinterpret_cast<T*>(&mData[0]);
    assert(ptr);
    return *ptr;
  }

  const T& get() const noexcept {
    return const_cast<LazyObject*>(this)->get();
  }
};

struct StaticDAGExecutorBFS {

  SerialUnorderedExecutor* mUnordExec;


  template <typename DAG>
  void executeFromHost(DAG* dag) {

    using NodePtr = typename DAG::NodePtr;

    // using LevelBag = std::vector<NodePtr>;
    // set needed to eliminate duplicates when populating the next level (or
    // frontier) using the outgoing edges of the current level (or frontier)
    using LevelBag = std::unordered_set<NodePtr>;

    LevelBag currLevel;
    LevelBag nextLevel;

    LevelBag* currPtr = &currLevel;
    LevelBag* nextPtr = &nextLevel;

    dag->forEachSource( [&] (NodePtr n) {
          // currLevel.emplace_back(n);
          currLevel.emplace(n);
        });

    while (!currPtr->empty()) {

      auto batchState = mUnordExec->startBatch();

      for (NodePtr n: *currPtr) {
        const auto& nodeData = dag->nodeData(n);
        mUnordExec->addToBatch(batchState, nodeData);
      }

      auto th = mUnordExec->launchBatch(batchState);

      // prepare next level
      nextPtr->clear();
      for (NodePtr n: *currPtr) {
        for (NodePtr succ: dag->successors(n)) {
          // nextPtr->emplace_back(succ);
          nextPtr->emplace(succ);
        }
      }

      std::swap(currPtr, nextPtr);

      mUnordExec->waitOnTask(th);
    }
  }


  template <typename DAG>
  void executeFromCP(DAG* dag) {

    using NodePtr = typename DAG::NodePtr;

    // using LevelBag = std::vector<NodePtr>;
    // set needed to eliminate duplicates when populating the next level (or
    // frontier) using the outgoing edges of the current level (or frontier)
    using LevelBag = std::unordered_set<NodePtr>;

    LevelBag currLevel;
    LevelBag nextLevel;

    LevelBag* currPtr = &currLevel;
    LevelBag* nextPtr = &nextLevel;

    dag->forEachSource([&] (NodePtr n) {
          currLevel.emplace(n);
        });

    auto prevBatchSig = impl::NULL_SIGNAL;

    bool first = true;
    while (!currPtr->empty()) {

      using BatchState = SerialUnorderedExecutor::BatchState;
      LazyObject<BatchState> batchState;

      if (first) {
        first = false;
        batchState.init(mUnordExec->startBatch());
      } else {
        batchState.init(mUnordExec->startBatchWithDep(prevBatchSig));
      }

      for (NodePtr n: *currPtr) {
        auto& nodeData = dag->nodeData(n);
        mUnordExec->addToBatch(batchState.get(), nodeData);
      }

      auto taskHand = mUnordExec->launchBatch(batchState.get());
      prevBatchSig = taskHand.mSignal;

      // prepare next level
      nextPtr->clear();
      for (NodePtr n: *currPtr) {
        for (NodePtr succ: dag->successors(n)) {
          nextPtr->emplace(succ);
        }
      }

      std::swap(currPtr, nextPtr);
    }

    if (prevBatchSig != impl::NULL_SIGNAL) {
      impl::waitOnSignalRelaxed(prevBatchSig);
    }

  }
};



} // end namespace dagr

#endif// DAGEE_INCLUDE_DAGR__DAGEXECUTOR_H_
