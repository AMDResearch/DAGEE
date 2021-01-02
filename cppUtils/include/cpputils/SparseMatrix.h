// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef INCLUDE_CPPUTILS_SPARSE_MATRX_H_
#define INCLUDE_CPPUTILS_SPARSE_MATRX_H_

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <cassert>

#include <boost/iterator/counting_iterator.hpp>

#include "cpputils/CheckStatus.h"

namespace cpputils {

// TODO: add functionality to convert to symmetric
template <typename I = uint32_t, typename E = double>
class GenericSparseMatrix {
 protected:
  size_t m_numNodes = 0;
  size_t m_numEdges = 0;

  using AdjPair = std::pair<I, E>;
  using AdjVec = std::vector<AdjPair>;

  std::vector<AdjVec> m_adjVecs;

  void alloc(size_t numNodes, size_t numEdges) {
    m_numNodes = numNodes;
    m_numEdges = numEdges;

    m_adjVecs.resize(m_numNodes);
  }

  void addEdge(const I& src, const I& dst, const E& ed) {
    assert(src < m_numNodes && "src ID out of bounds");
    assert(dst < m_numNodes && "dst ID out of bounds");

    m_adjVecs[src].emplace_back(dst, ed);
  }

  template <typename I1, typename E1>
  static void readTriple(const std::string& line, I1& first, I1& second, E1& third) {
    std::istringstream ls(line);
    ls >> first;
    ls >> second;
    ls >> third;
  }

 public:
  using adjIterator = typename AdjVec::iterator;
  using const_adjIterator = typename AdjVec::const_iterator;

  size_t numNodes(void) const { return m_numNodes; }
  size_t numEdges(void) const { return m_numEdges; }

  adjIterator adjBegin(size_t i) {
    assert(i < numNodes() && "adjBegin: index out of bound");
    return m_adjVecs[i].begin();
  }

  const_adjIterator adjBegin(size_t i) const {
    assert(i < numNodes() && "adjBegin: index out of bound");
    return m_adjVecs[i].cbegin();
  }

  adjIterator adjEnd(size_t i) {
    assert(i < numNodes() && "adjBegin: index out of bound");
    return m_adjVecs[i].end();
  }

  const_adjIterator adjEnd(size_t i) const {
    assert(i < numNodes() && "adjBegin: index out of bound");
    return m_adjVecs[i].cend();
  }

  I& edgeDst(adjIterator i) { return i->first; }

  const I& edgeDst(const_adjIterator i) const { return i->first; }

  E& edgeData(adjIterator i) { return i->second; }

  const E& edgeData(const_adjIterator i) const { return i->second; }

  void readMatrixMarket(const std::string& fileName) {
    constexpr char MATRIX_MARKET_COMMENT = '%';

    std::ifstream ifs(fileName);

    failIf(!ifs.is_open() || !ifs.good(), "Failed to open file:" + fileName);

    bool first = true;

    for (std::string line; std::getline(ifs, line);) {
      if (line[0] == MATRIX_MARKET_COMMENT) {
        continue;
      }

      if (first) {
        first = false;

        size_t srcNodes = 0;
        size_t dstNodes = 0;
        size_t numEdges = 0;

        readTriple(line, srcNodes, dstNodes, numEdges);

        assert(srcNodes > 0);
        assert(dstNodes > 0);

        size_t numNodes = std::max(srcNodes, dstNodes);

        this->alloc(numNodes, numEdges);

      } else {
        I src = 0;
        I dst = 0;
        E ed = 0;

        readTriple(line, src, dst, ed);

        this->addEdge(src - 1, dst - 1, ed);
      }
    }
  }

  void sortAdj(void) {
    // TODO
  }

  void convToLower(void) {
    // TODO
  }

  void convToUpper(void) {
    // TODO
  }

  GenericSparseMatrix transpose(void) const {
    // TODO
  }
};

enum class SpMat { CSR = 0, CSC };

template <typename I = uint32_t, typename E = double>
class CompressedMatrix {
 protected:
  size_t m_numNodes = 0;
  size_t m_numEdges = 0;

  std::vector<I> m_index;
  std::vector<I> m_adj;
  std::vector<E> m_edgeData;

  void alloc(size_t numNodes, size_t numEdges) {
    m_numNodes = numNodes;
    m_numEdges = numEdges;

    m_index.reserve(m_numNodes);
    m_adj.reserve(m_numEdges);
    m_edgeData.reserve(m_numEdges);
  }

  void dealloc() {
    m_index.clear();
    m_adj.clear();
    m_edgeData.clear();

    m_numNodes = 0ul;
    m_numEdges = 0ul;
  }

  // using IndexAlloc = std::allocator<I>;
  // using EdgeAlloc = std::allocator<E>;
  //
  // I* m_index = nullptr;
  // I* m_adj = nullptr;
  // E* m_edgeData = nullptr;

  // IndexAlloc m_indexAlloc;
  // EdgeAlloc m_edgeAlloc;

  // void alloc(size_t numNodes, size_t numEdges) {
  //
  // m_numNodes = numNodes;
  // m_numEdges = numEdges;
  //
  // m_index = m_indexAlloc.allocate(m_numNodes + 1);
  // m_adj = m_indexAlloc.allocate(m_numEdges);
  // m_edgeData = m_edgeAlloc.allocate(m_numEdges);
  // }
  //
  // void dealloc() {
  // m_indexAlloc.deallocate(m_index, m_numNodes + 1);
  // m_indexAlloc.deallocate(m_adj, m_numEdges);
  // m_edgeAlloc.deallocate(m_edgeData, m_numEdges);
  //
  // m_numNodes = 0ul;
  // m_numEdges = 0ul;
  //
  // m_index = nullptr;
  // m_adj = nullptr;
  // m_edgeData = nullptr;
  // }

  /*
  template <typename _I1, typename _E1>
  void copyFrom(const GenericSparseMatrix<_I1, _E1>& spMat) {

    alloc(spMat.numNodes(), spMat.numEdges());

    m_index[0] = 0;
    size_t edgeIndex = 0;

    for (size_t i = 0; i < spMat.numNodes(); ++i) {

      I adjSize = 0;

      for (auto e = spMat.adjBegin(i), end_e = spMat.adjEnd(i); e != end_e; ++e) {

        m_adj[edgeIndex] = spMat.getEdgeDst(e);
        m_edgeData[edgeIndex] = spMat.getEdgeData(e);

      }

      m_index[i+1] = adjSize;
    }

    assert(edgeIndex == m_numEdges && "CompressedMatrix::copyFrom: Didn't copy right amount of
  edges");
  }
  */

  template <typename _I1, typename _E1>
  void copyFrom(const GenericSparseMatrix<_I1, _E1>& spMat) {
    alloc(spMat.numNodes(), spMat.numEdges());

    m_index.push_back(0);
    for (size_t i = 0; i < spMat.numNodes(); ++i) {
      I adjSize = m_index[i];

      for (auto e = spMat.adjBegin(i), end_e = spMat.adjEnd(i); e != end_e; ++e) {
        m_adj.push_back(spMat.edgeDst(e));
        m_edgeData.push_back(spMat.edgeData(e));

        ++adjSize;
      }

      m_index.push_back(adjSize);
    }

    assert(m_index.size() == numNodes() + 1 && "copyFrom: Wrong number of nodes");
    assert(m_adj.size() == numEdges() && "copyFrom: wrong number of edges");
    assert(m_edgeData.size() == numEdges() && "copyFrom: wrong number of edges");
  }

  template <typename Ret>
  Ret adjBeginImpl(const I& src) {
    assert(src < numNodes());
    return Ret(m_index[src]);
  }

  template <typename Ret>
  Ret adjEndImpl(const I& src) {
    assert(src < numNodes() && (src + 1) <= numNodes());
    return Ret(m_index[src + 1]);
  }

  template <typename Ret, typename It>
  Ret& edgeDstImpl(It iter) {
    assert(*iter < m_adj.size());
    return m_adj[*iter];
  }

  template <typename Ret, typename It>
  Ret& edgeDataImpl(It iter) {
    assert(*iter < m_adj.size());
    return m_edgeData[*iter];
  }

 public:
  using adjIterator = boost::counting_iterator<uint64_t>;
  using const_adjIterator = boost::counting_iterator<uint64_t>;

  template <typename _I1, typename _E1>
  CompressedMatrix(const GenericSparseMatrix<_I1, _E1>& spMat) {
    copyFrom(spMat);
  }

  ~CompressedMatrix(void) { dealloc(); }

  size_t numNodes(void) const { return m_numNodes; }
  size_t numEdges(void) const { return m_numEdges; }

  adjIterator adjBegin(const I& src) { return adjBeginImpl<adjIterator>(src); }

  const_adjIterator adjBegin(const I& src) const {
    return const_cast<CompressedMatrix*>(this)->adjBeginImpl<const_adjIterator>(src);
  }

  adjIterator adjEnd(const I& src) { return adjEndImpl<adjIterator>(src); }

  const_adjIterator adjEnd(const I& src) const {
    return const_cast<CompressedMatrix*>(this)->adjEndImpl<const_adjIterator>(src);
  }

  I& edgeDst(adjIterator i) { return edgeDstImpl<I>(i); }

  const I& edgeDst(const_adjIterator i) const {
    return const_cast<CompressedMatrix*>(this)->edgeDstImpl<const I>(i);
  }

  E& edgeData(adjIterator i) { return edgeDataImpl<E>(i); }

  const E& edgeData(const_adjIterator i) const {
    return const_cast<CompressedMatrix*>(this)->edgeDataImpl<const E>(i);
  }
};

} // end namespace cpputils
#endif // INCLUDE_CPPUTILS_SPARSE_MATRX_H_
