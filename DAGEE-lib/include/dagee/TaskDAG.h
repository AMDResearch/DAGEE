// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_TASK_DAG_H
#define DAGEE_INCLUDE_DAGEE_TASK_DAG_H

#include "dagee/AllocFactory.h"

#include <atomic>
#include <type_traits>
#include <vector>

namespace dagee {

using AtomicCounter = std::atomic<unsigned>;
using AtomicCounterValue = unsigned;

template <typename T, typename AF>
using AdjList = typename AF::template Vec<T>;

template <typename T = uint32_t>
struct IDbase {
  using IDty = T;

  IDbase(const T& id) : mID(id) {}

  const T& ID(void) const { return mID; }

 protected:
  T mID;
  T& ID(void) { return mID; }
};

template <>
struct IDbase<void> {
  void ID(void) { return; }
};

/*
 * TODO: refactor into base classes such as successor data, predecessor data,
 * and function data, then combine them to make node data pull, push, inout
 */

template <typename D>
struct NodeBase {
  D mData;

  template <typename... Args>
  NodeBase(Args&&... args) : mData(std::forward<Args>(args)...) {}

  D& data(void) { return mData; }

  const D& data(void) const { return mData; }
};

template <typename Derived, typename AllocFactory>
struct NodeSuccBase {
  using AdjListTy = AdjList<Derived*, AllocFactory>;

  AdjListTy mSuccList;

  void addSucc(Derived* n) {
    assert(n && "null pointer arg");
    assert(!hasSucc(n) && "duplicate edge");
    mSuccList.push_back(n);
  }

  bool hasSucc(Derived* n) const {
    assert(n && "null pointer arg");
    return (std::find(mSuccList.cbegin(), mSuccList.cend(), n) != mSuccList.cend());
  }

  const AdjListTy& successors(void) const { return mSuccList; }

  bool isSink(void) const { return mSuccList.cbegin() == mSuccList.cend(); }
};

template <typename Derived, typename AllocFactory>
struct NodePredBase {
  using AdjListTy = AdjList<Derived*, AllocFactory>;

  AdjListTy mPredList;

  void addPred(Derived* n) {
    assert(n && "null pointer arg");
    assert(!hasPred(n) && "duplicate edge");
    mPredList.push_back(n);
  }

  bool hasPred(Derived* n) const {
    assert(n && "null pointer arg");
    return (std::find(mPredList.cbegin(), mPredList.cend(), n) != mPredList.cend());
  }

  const AdjListTy& predecessors(void) const { return mPredList; }
};

template <typename __UNUSED = void>
struct PredCountBase {
  //! increment the number of predecessors as the edges are added to the graph
  AtomicCounter mNumPred;
  //! a shadow copy of mNumPred used for forEachNode_TopoOrder. It  allows resetting count and
  //! reusing the DAG when needed
  AtomicCounter mDepCounter;

  // using T = typename AtomicCounter::value_type;

  PredCountBase(void) : mNumPred(0), mDepCounter(0) {}

  bool isSrc(void) const { return mNumPred == 0; }

  void addPred(PredCountBase* b) { ++mNumPred; }

  void resetDepCounter(void) {
    mDepCounter.store(mNumPred.load(std::memory_order_relaxed), std::memory_order_release);
  }

  AtomicCounterValue decrementDepCounter(void) { return --mDepCounter; }

  AtomicCounterValue depCounter(void) const { return mDepCounter; }
};

template <typename D, typename AllocFactory>
struct NodePush : public PredCountBase<>,
                  public NodeBase<D>,
                  public NodeSuccBase<NodePush<D, AllocFactory>, AllocFactory> {
  using PCbase = PredCountBase<>;
  using NDbase = NodeBase<D>;
  using SuccBase = NodeSuccBase<NodePush, AllocFactory>;

  template <typename... Args>
  explicit NodePush(Args&&... args) : PCbase(), NDbase(std::forward<Args>(args)...), SuccBase() {}
};

template <typename D, typename AllocFactory>
struct NodePull : public NodeBase<D>, public NodePredBase<NodePull<D, AllocFactory>, AllocFactory> {
  using NDbase = NodeBase<D>;
  using PredBase = NodePredBase<NodePull, AllocFactory>;

  bool mDone = false;

  template <typename... Args>
  explicit NodePull(Args&&... args) : NDbase(std::forward<Args>(args)...), PredBase() {}

  void addSucc(NodePull*) const {}

  // Can store a 1-bit flag on the pointer to indicate whether the predecessor
  // is
  // done, to avoid re-reading mDone flag.
  bool isSrc(void) const {
    for (NodePull* p : PredBase::mPredList) {
      if (!p->mDone) {
        return false;
      }
    }
    return true;
  }
};

template <typename D, typename AllocFactory>
struct NodeInOut : public PredCountBase<>,
                   public NodeBase<D>,
                   public NodeSuccBase<NodeInOut<D, AllocFactory>, AllocFactory>,
                   public NodePredBase<NodeInOut<D, AllocFactory>, AllocFactory> {
  using PCbase = PredCountBase<>;
  using NDbase = NodeBase<D>;
  using SuccBase = NodeSuccBase<NodeInOut, AllocFactory>;
  using PredBase = NodePredBase<NodeInOut, AllocFactory>;

  using AdjListTy = typename SuccBase::AdjListTy; // XXX: Hack to work around the fact that SuccBase
                                                  // and PredBase define AdjListTy separately

  AtomicCounter mNumPred;

  template <typename... Args>
  explicit NodeInOut(Args&&... args)
      : PCbase(), NDbase(std::forward<Args>(args)...), SuccBase(), PredBase() {}

  void addPred(NodeInOut* b) {
    PredBase::addPred(b);
    PCbase::addPred(b);
  }

  bool isSrc(void) const { return PCbase::isSrc(); }

  void addSucc(NodeInOut* a) { SuccBase::addSucc(a); }
};

namespace impl {
// TODO(amber): simplify this choosing logic
template <typename D, typename AllocFactory, bool STORE_PRED, bool STORE_SUCC>
struct ChooseNodeType {};

template <typename D, typename AllocFactory>
struct ChooseNodeType<D, AllocFactory, true, true> {
  using type = NodeInOut<D, AllocFactory>;
};

template <typename D, typename AllocFactory>
struct ChooseNodeType<D, AllocFactory, false, true> {
  using type = NodePush<D, AllocFactory>;
};

template <typename D, typename AllocFactory>
struct ChooseNodeType<D, AllocFactory, true, false> {
  using type = NodePull<D, AllocFactory>;
};
} // namespace impl

template <typename D, typename AllocFactory = dagee::StdAllocatorFactory<>, bool STORE_PRED = true,
          bool STORE_SUCC = false>
struct DAGbase {
 public:
  using WithPred = DAGbase<D, AllocFactory, true, false>;
  using WithSucc = DAGbase<D, AllocFactory, false, true>;
  using WithPredSucc = DAGbase<D, AllocFactory, true, true>;
  template <typename AF>
  using withAllocFactory = DAGbase<D, AF, STORE_PRED, STORE_SUCC>;

  using Node = typename impl::ChooseNodeType<D, AllocFactory, STORE_PRED, STORE_SUCC>::type;
  using NodeData = D;
  using NodePtr = Node*;
  using NodeCptr = const Node*;
  using IDty = size_t;

 protected:
  using NodeAlloc = typename AllocFactory::template FixedSizeAlloc<Node>;
  using NodeCont = typename AllocFactory::template Vec<NodePtr>;
  using NodeAllocTraits = dagee::AllocTraits<NodeAlloc>;
  using NodeDeq = typename AllocFactory::template Deque<NodePtr>;

  // TODO: add ID optionally
  // IDty mID;
  NodeAlloc mNodeAlloc;
  NodeCont mAllNodes;

  void destroyNode(NodePtr t) {
    assert(t && "arg must be non-null");
    NodeAllocTraits::destroy(mNodeAlloc, t);
    NodeAllocTraits::deallocate(mNodeAlloc, t, 1);
  }

  void destroyAllNodes(void) {
    for (auto t : mAllNodes) {
      destroyNode(t);
    }
    mAllNodes.clear();
  }

 public:
  // TODO: add this later
  // explicit DAGbase(const IDty& id): mID(id) {}

  ~DAGbase(void) { destroyAllNodes(); }

  void clear(void) { destroyAllNodes(); }

  // const IDty& getID(void) const { return mID; }

  // Dummy var S is needed because in case when STORE_SUCC is false, for SFINAE
  // to
  // apply, enable_if<false> should occur in the "immediate context" of the
  // template <typename S>
  template <bool S = STORE_SUCC, typename std::enable_if<S, int>::type = 0>
  const typename Node::AdjListTy& successors(const Node* a) const {
    return a->successors();
  }

  template <bool P = STORE_PRED, typename std::enable_if<P, int>::type = 0>
  const typename Node::AdjListTy& predecessors(const Node* a) const {
    return a->predecessors();
  }

  template <bool P = STORE_PRED, bool S = STORE_SUCC,
            typename std::enable_if<P && S, int>::type = 0>
  bool hasEdge(const NodePtr& a, const NodePtr& b) const {
    return a->hasSucc(b) && b->hasPred(a);
  }

  template <bool P = STORE_PRED, bool S = STORE_SUCC,
            typename std::enable_if<P && !S, int>::type = 0>
  bool hasEdge(const NodePtr& a, const NodePtr& b) const {
    return b->hasPred(a);
  }

  template <bool P = STORE_PRED, bool S = STORE_SUCC,
            typename std::enable_if<!P && S, int>::type = 0>
  bool hasEdge(const NodePtr& a, const NodePtr& b) const {
    return a->hasSucc(b);
  }

  const D& nodeData(const Node* node) const { return node->data(); }

  D& nodeData(Node* node) { return node->data(); }

  template <typename... Args>
  Node* addNode(Args&&... args) {
    Node* t = NodeAllocTraits::allocate(mNodeAlloc, 1);
    assert(t && "node allocation failed");
    NodeAllocTraits::construct(mNodeAlloc, t, std::forward<Args>(args)...);
    mAllNodes.push_back(t);
    return t;
  }

  void addEdge(NodePtr a, NodePtr b) {
    assert(a && b && "both args should be non-null");
    assert(a != b && "cannot add self edge, i.e., src==dst");
    a->addSucc(b);
    b->addPred(a);
  }

  void addFanOutEdges(NodePtr a, std::initializer_list<NodePtr> lb) {
    for (auto b : lb) {
      addEdge(a, b);
    }
  }

  void addFanInEdges(std::initializer_list<NodePtr> la, NodePtr b) {
    for (auto a : la) {
      addEdge(a, b);
    }
  }

  void addEdgeIfAbsent(NodePtr a, NodePtr b) {
    assert(a && b && "both args should be non-null");
    assert(a != b && "cannot add self edge, i.e., src==dst");
    if (!hasEdge(a, b)) {
      addEdge(a, b);
    }
  }

  bool isSrc(NodePtr n) const { return n->isSrc(); }

  template <bool S = STORE_SUCC>
  typename std::enable_if<S, bool>::type isSink(NodePtr n) const {
    return n->isSink();
  }

  template <typename F>
  void forEachNode(F&& func) {
    for (NodePtr p : mAllNodes) {
      assert(p && "found null node in mAllNodes");
      func(p);
    }
  }

  template <typename F>
  void forEachSource(F&& func) {
    this->forEachNode([&func, this](NodePtr p) {
      if (isSrc(p)) {
        func(p);
      }
    });
  }

  template <typename F>
  void forEachSink(F&& func) {
    this->forEachNode([&func](NodePtr p) {
      if (p->isSink()) {
        func(p);
      }
    });
  }
  template <typename F, bool S = STORE_SUCC>
  void forEachNode_TopoOrder(F&& func) {
    NodeDeq nodeQ;

    this->forEachNode([&, this](NodePtr p) {
      if (isSrc(p)) {
        nodeQ.emplace_back(p);
      }
      p->resetDepCounter();
    });

    while (!nodeQ.empty()) {
      auto p = nodeQ.back();
      nodeQ.pop_back();

      assert(p->depCounter() == 0 && "invariant violation every node in nodeQ must be a source");
      func(p);

      for (NodePtr d : this->successors(p)) {
        assert(d && "found null node in successors of a source");
        if (d->decrementDepCounter() == 0) {
          nodeQ.emplace_back(d);
        }
      }
    }
  }

  /*
  template <typename C, bool S = STORE_SUCC>
  typename std::enable_if<S>::type findNextSources(Node* src, C& nextSources) {
    assert(src && src->isSrc() && "source node null or not a source");

    for (Node* t : src->mSuccList) {
      assert(t && "found null node in mSuccList of a source");
      if (--t->mNumPred == 0) {
        nextSources.push_back(t);
      }
    }
  }
  */

  std::pair<size_t, size_t> size(void) const {
    size_t numNodes = 0;
    size_t numEdges = 0;
    for (auto t : mAllNodes) {
      ++numNodes;
      numEdges += t->successors().size();
    }

    return std::make_pair(numNodes, numEdges);
  }
};

namespace impl {

template <typename In, size_t Index, typename Arg0, typename... Args>
struct GetTypeIdImpl {
  constexpr static const size_t value =
      std::is_same<In, Arg0>::value ? Index : GetTypeIdImpl<In, Index + 1, Args...>::value;
};
template <typename In, size_t Index, typename LastArg>
struct GetTypeIdImpl<In, Index, LastArg> {
  constexpr static const size_t value = std::is_same<In, LastArg>::value ? Index : Index + 1;
};

template <typename In, typename... Args>
struct GetTypeId {
  constexpr static const size_t value =
      GetTypeIdImpl<typename std::remove_cv<In>::type, 0ul, Args...>::value;
  static_assert(value < sizeof...(Args), "Invalid input type searched in parameter pack");
};

template <typename T, typename... DataTypes>
constexpr size_t getTypeId(std::tuple<DataTypes...>*) {
  return GetTypeId<T, DataTypes...>::value;
}

template <typename I, I... Ints>
struct IntSeq {
  constexpr static const size_t size = sizeof...(Ints);
};

template <typename I, I Beg, I End, I... Prefix>
struct MakeIntSeqHelper {
  using type =
      typename std::conditional<(Beg < End),
                                // Beg < End, so grow the sequence
                                typename MakeIntSeqHelper<I, Beg + 1, End, Prefix..., Beg>::type,
                                // No more growing, just return Prefix
                                IntSeq<I, Prefix...> >::type;
};

template <typename I, I Beg, I... Prefix>
struct MakeIntSeqHelper<I, Beg, Beg, Prefix...> {
  using type = IntSeq<I, Prefix...>;
};

template <typename I, I Beg, I End>
using MakeIntSeq = typename MakeIntSeqHelper<I, Beg, End>::type;

template <size_t N>
using MakeIndexSeq = MakeIntSeq<size_t, 0, N>;

template <typename... Args>
using MakeIndexSeqFor = MakeIndexSeq<sizeof...(Args)>;

/*
 * Some hackery to encode an enum (indicating type)
 * in a pointer. Works because if pointer is aligned to 32-bit or 64-bit
 * boundary, the lower 2 or 3 bits are always 0.
 */
template <typename... DataTypes>
struct MultiTypePtr {
  using GenericPtr = std::uint64_t;

  constexpr static const size_t MAX_TYPES = alignof(std::uintptr_t);
  constexpr static const size_t MASK = MAX_TYPES - 1;

  static_assert(sizeof...(DataTypes) <= MAX_TYPES, "Can't uniquely encode types");

  GenericPtr mData;

  template <typename T>
  explicit MultiTypePtr(T* ptr) noexcept : mData(reinterpret_cast<GenericPtr>(ptr)) {
    constexpr size_t ID = GetTypeId<T, DataTypes...>::value;
    mData |= ID;
  }

  size_t id() const noexcept { return mData & MASK; }

  template <typename T>
  T* ptr() noexcept {
    return reinterpret_cast<T*>(mData & ~(MASK));
  }

  template <typename T>
  const T* ptr() const {
    return reinterpret_cast<const T*>(mData & ~MASK);
  }

  template <typename F, size_t... Indices>
  void applyByTypeImpl(F&& func, IntSeq<size_t, Indices...>) {
    const auto ID = id();
    (void)(int[]){(((ID == Indices) ? func(*ptr<DataTypes>()) : void()), 0)...};
  }

  template <typename F>
  void apply(F&& func) {
    applyByTypeImpl(std::forward<F>(func), MakeIndexSeqFor<DataTypes...>());
  }

  /*
  template <typename F>
  void apply(F&& func) const {
    applyByTypeImpl(std::forward<F>(func), MakeIndexSeqFor<DataTypes...>());
  }
  */
};
} // end namespace impl

template <typename AllocFactory = dagee::StdAllocatorFactory<>, bool STORE_PRED = true,
          bool STORE_SUCC = false, typename... DataTypes>
class MixedTaskDag
    : public DAGbase<impl::MultiTypePtr<DataTypes...>, AllocFactory, STORE_PRED, STORE_SUCC> {
  using GenericNodeData = impl::MultiTypePtr<DataTypes...>;
  using Base = DAGbase<GenericNodeData, AllocFactory, STORE_PRED, STORE_SUCC>;

 public:
  using WithSucc = MixedTaskDag<AllocFactory, false, true, DataTypes...>;
  using WithPred = MixedTaskDag<AllocFactory, true, false, DataTypes...>;
  using WithPredSucc = MixedTaskDag<AllocFactory, true, true, DataTypes...>;

  template <typename AF>
  using withAllocFactory = MixedTaskDag<AF, STORE_PRED, STORE_SUCC, DataTypes...>;

  using typename Base::Node;
  using typename Base::NodeCptr;
  using typename Base::NodeData;
  using typename Base::NodePtr;

 private:
  template <typename T>
  using AllocTy = typename AllocFactory::template FixedSizeAlloc<T>;
  using AllocTuple = std::tuple<AllocTy<DataTypes>...>;

  AllocTuple mAllocators;

  // TODO(amber): use std::forward with D0&&
  template <typename A, typename E>
  static GenericNodeData allocateNodeData(A& alloc, const E& elem) {
    using Atraits = dagee::AllocTraits<A>;
    E* ePtr = Atraits::allocate(alloc, 1);
    assert(ePtr && "allocateNodeData failed");
    Atraits::construct(alloc, ePtr, elem);
    return GenericNodeData(ePtr);
  }

  template <typename A, typename E>
  static void deallocateNodeData(A& alloc, E* ePtr) {
    assert(ePtr && "invalid pointer in deallocateNodeData");
    using Atraits = dagee::AllocTraits<A>;
    Atraits::destroy(alloc, ePtr);
    Atraits::deallocate(alloc, ePtr, 1);
  }

  template <typename A, typename E>
  NodePtr addNodeImpl(A& alloc, const E& elem) {
    GenericNodeData gd = allocateNodeData(alloc, elem);
    return Base::addNode(gd);
  }

  template <size_t... Indices>
  void deleteNodeImpl(GenericNodeData& gd, impl::IntSeq<size_t, Indices...>) {
    const size_t id = gd.id();

    (void)(int[]){(((id == Indices) ? deallocateNodeData(std::get<Indices>(mAllocators),
                                                         gd.template ptr<DataTypes>())
                                    : void(0)),
                   0)...};
  }

  void destroyAllNodeData() {
    Base::forEachNode([this](NodePtr p) {
      GenericNodeData gd = Base::nodeData(p);
      deleteNodeImpl(gd, impl::MakeIndexSeqFor<DataTypes...>());
    });
  }

 public:
  ~MixedTaskDag() { destroyAllNodeData(); }

  // TODO(amber): use std::forward with D0&&
  template <typename D>
  NodePtr addNode(const D& d) {
    constexpr size_t ID = impl::GetTypeId<D, DataTypes...>::value;
    return addNodeImpl(std::get<ID>(mAllocators), d);
  }

  template <typename F>
  void applyToNodeData(NodePtr n, F&& func) {
    assert(n && "null node pointer");

    GenericNodeData& gd = Base::nodeData(n);
    gd.apply(std::forward<F>(func));
  }

  template <typename F>
  void applyToNodeData(NodeCptr cn, F&& func) {
    assert(cn && "null node pointer");

    NodePtr n = const_cast<NodePtr>(cn);

    GenericNodeData& gd = Base::nodeData(n);
    gd.apply(std::forward<F>(func));
  }

  /*
  template <typename E>
  const E& nodeData(NodeCptr node) const {
    const GenericNodeData& x = Base::nodeData(node);
    return *(x.ptr<E>());
  }

  E& nodeData(NodePtr node) {
    GenericNodeData& x = Base::nodeData(node);
    return *(x.ptr<E>());
  }
  */
};

/**
 * DAG class constructor must take a size_t ID as its first arg
 * DAG class must implement size_t getID() method
 */
template <typename DAG_tp, typename AllocFactory, bool DAG_ID_FLAG = false>
class DAGmanager {
 public:
  using DAG = DAG_tp;
  using DAGptr = DAG*;

  using withID = DAGmanager<DAG, AllocFactory, true>;
  template <typename AF>
  using withAllocFactory = DAGmanager<DAG_tp, AF, DAG_ID_FLAG>;

 protected:
  using IDty = typename DAG::IDty;

  // XXX: assuming the DAG allocator can be used for vector
  using DAGalloc = typename AllocFactory::template FixedSizeAlloc<DAG>;
  using DAGallocTraits = dagee::AllocTraits<DAGalloc>;
  using DAGmap = typename AllocFactory::template Vec<DAGptr>;

  DAGalloc mDAGalloc;
  DAGmap mDAGmap;

  template <bool D = DAG_ID_FLAG>
  typename std::enable_if<D, IDty>::type genID(void) {
    auto sz = mDAGmap.size();
    mDAGmap.resize(sz + 1);
    return sz;
  }

  DAGptr allocDAG(void) {
    DAGptr d = DAGallocTraits::allocate(mDAGalloc, 1);
    assert(d && "node allocation failed");
    return d;
  }

  void freeDAG(DAGptr d) {
    assert(d && "arg must be non-null");
    DAGallocTraits::destroy(mDAGalloc, d);
    DAGallocTraits::deallocate(mDAGalloc, d, 1);
  }

  void destroyAllDAGs(void) {
    for (auto d : mDAGmap) {
      if (d) {
        freeDAG(d);
      }
    }

    mDAGmap.clear();
  }

 public:
  template <typename... Args, bool D = DAG_ID_FLAG>
  typename std::enable_if<D, DAGptr>::type makeDAG(Args&&... args) {
    auto id = genID();

    DAGptr d = allocDAG();
    DAGallocTraits::construct(mDAGalloc, d, id, std::forward<Args>(args)...);

    mDAGmap[id] = d;

    return d;
  }

  template <typename... Args, bool D = DAG_ID_FLAG>
  typename std::enable_if<!D, DAGptr>::type makeDAG(Args&&... args) {
    DAGptr d = allocDAG();
    DAGallocTraits::construct(mDAGalloc, d, std::forward<Args>(args)...);
    return d;
  }

  template <bool D = DAG_ID_FLAG>
  typename std::enable_if<D>::type destroyDAG(DAGptr d) {
    assert(d && "arg must be non-null");
    auto id = d->ID();

    freeDAG(d);

    mDAGmap[id] = nullptr;
  }

  template <bool D = DAG_ID_FLAG>
  typename std::enable_if<!D>::type destroyDAG(DAGptr d) {
    assert(!mDAGmap.empty());
    assert(std::find(mDAGmap.cbegin(), mDAGmap.cend(), d) != mDAGmap.cend() &&
           "DAG not found in DAGmanager");

    auto i = std::remove(mDAGmap.begin(), mDAGmap.end(), d);
    assert(*i == mDAGmap.back());
    mDAGmap.pop_back();

    freeDAG(d);
  }

  ~DAGmanager(void) { destroyAllDAGs(); }
};

} // end namespace dagee
#endif // DAGEE_INCLUDE_DAGEE_TASK_DAG_H

/*
 * XXX: Notes
 * Should give the option to the user to identify sources and sinks
 */
