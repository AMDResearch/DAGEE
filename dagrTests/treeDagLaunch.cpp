#include "kernels.h"

#include "dagr/dagExecutor.h"
#include "dagr/executor.h"

#include "dagee/TaskDAG.h"

#include "cpputils/CmdLine.h"
#include "cpputils/Timer.h"

#include <cassert>

#include <iostream>
#include <thread>

template <typename DAG, typename MakeTaskFunc>
void cojoinedTreeDag(DAG* dag, MakeTaskFunc&& makeTaskFunc, size_t levels, size_t degree) {
  assert(dag);

  using NodePtr = typename DAG::NodePtr;
  using NodeVec = std::vector<NodePtr>;

  NodeVec currVec;
  NodeVec nextVec;
  NodeVec* curr = &currVec;
  NodeVec* next = &nextVec;

  size_t numNodes = 0ul;
  size_t numEdges = 0ul;

  auto root = dag->addNode(makeTaskFunc());
  ++numNodes;
  curr->emplace_back(root);

  // create expanding tree, where every node has `degree` number of children
  for (size_t i = 0; i < levels; ++i) {
    assert(next->empty());
    for (auto* n : *curr) {
      for (size_t d = 0; d < degree; ++d) {
        auto* child = dag->addNode(makeTaskFunc());
        ++numNodes;
        dag->addEdge(n, child);
        ++numEdges;
        next->emplace_back(child);
      }
    }

    std::swap(curr, next);
    next->clear();
  }
  // create reducing tree, where `degree` parents in curr level share one child in
  // next level
  for (size_t i = 0; i < levels; ++i) {
    size_t nextLevelSz = curr->size() / degree;
    assert(nextLevelSz > 0);
    assert(next->empty());

    for (size_t c = 0; c < nextLevelSz; ++c) {
      auto* node = dag->addNode(makeTaskFunc());
      ++numNodes;
      next->emplace_back(node);
    }

    size_t nextIndex = 0ul;
    size_t counter = 0ul;

    for (auto* node : *curr) {
      assert(nextIndex < next->size());
      dag->addEdge(node, (*next)[nextIndex]);
      ++numEdges;
      ++counter;

      if (counter % degree == 0) {
        ++nextIndex;
      }
    }

    std::swap(curr, next);
    next->clear();
  }

  std::printf("Tree Graph created, numNodes = %zu, numEdges = %zu\n", numNodes, numEdges);
}

int main(int argc, char** argv) {
  namespace cl = cpputils::cmdline;

  cl::Option<size_t> numLevelsOpt('l', "Number of levels in tree", 10ul);
  cl::Option<size_t> degreeOpt('d', "Degree of each node", 2ul);
  cl::Parser parser({&numLevelsOpt, &degreeOpt});
  parser.parse(argc, argv);

  dagr::RuntimeState S;
  dagr::GpuExecutionResource er(S.gpuAgent(0));

  auto* kinfoNoWork = er.kernInfoState().registerKernel<>(&emptyKern);

  dagr::DeviceMemManager dMemMgr(er.regionState().coarseRegion(0));

  dagr::SerialUnorderedExecutor unordExec(&er, 4ul);
  dagr::StaticDAGExecutorBFS dagExec{&unordExec};

  using TaskDag = dagee::DAGbase<dagr::GpuKernInstance>::WithSucc;

  TaskDag dag;

  cojoinedTreeDag(&dag, [&]() { return unordExec.makeTask(dim3(1), dim3(1), kinfoNoWork); },
                  size_t(numLevelsOpt), size_t(degreeOpt));

  constexpr static const size_t NUM_REP = 10ul;

  for (size_t i = 0; i < NUM_REP; ++i) {
    cpputils::Timer t0("DAG", "Execute From Host", true);
    dagExec.executeFromHost(&dag);
    t0.stop();
  }

  for (size_t i = 0; i < NUM_REP; ++i) {
    cpputils::Timer t1("DAG", "Execute From CP", true);
    dagExec.executeFromCP(&dag);
    t1.stop();
  }

  return 0;
}
