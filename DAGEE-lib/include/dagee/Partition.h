// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#ifndef DAGEE_INCLUDE_DAGEE_PARTITION_H
#define DAGEE_INCLUDE_DAGEE_PARTITION_H

#include "dagee/ATMIcoreDef.h"
#include "dagee/AllocFactory.h"
#include "dagee/DeviceAlloc.h"

#include <functional>

namespace dagee {

// TODO: We should serialize host data into a contiguous buffer just like device data

template <typename T>
struct DataBlock {
  T* mPtr;
  size_t mSize;

  template <typename U>
  operator DataBlock<U>(void) {
    return DataBlock<U>{reinterpret_cast<U*>(mPtr), mSize};
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<> >
struct PartHostData {
  using Dblk = DataBlock<Byte>;
  using VarName = typename AllocFactory::Str;
  using DataBlockMap = typename AllocFactory::template HashMap<VarName, Dblk>;
  using const_iterator = typename DataBlockMap::const_iterator;

 protected:
  DataBlockMap mDataBlocks;

 public:
  /*
  explicit PartHostData(std::initializer_list<DataBlock> ptrList): mDataBlocks(ptrList.begin(),
  ptrList.end())
  {}
  */

  template <typename S, typename T>
  void addDataBlock(const S& name, T* ptr, size_t numElem) {
    assert(mDataBlocks.find(name) == mDataBlocks.cend());
    mDataBlocks.emplace(name, Dblk{reinterpret_cast<BytePtr>(ptr), numElem * sizeof(T)});
  }

  template <typename R, typename S>
  R* blkPtr(const S& name) {
    assert(mDataBlocks.find(name) != mDataBlocks.cend());
    return reinterpret_cast<R*>(mDataBlocks[name].mPtr);
  }

  const_iterator begin(void) const { return mDataBlocks.begin(); }
  const_iterator end(void) const { return mDataBlocks.end(); }

  const VarName& varName(const_iterator i) const { return i->first; }

  template <typename R>
  static R* blkPtr(const_iterator i) {
    return reinterpret_cast<R*>(i->second.mPtr);
  }

  static size_t blkSz(const_iterator i) { return i->second.mSize; }
  /*
  template <typename S>
  const DataBlock& dataBlock(const S& name) const {
    assert(mDataBlocks.find(name) != mDataBlocks.cend());
    return mDataBlocks[name];
  }
  */

  /*
  const std::vector<DataBlock>& dataBlocks(void) const {
    return mDataBlocks;
  }
  */

  size_t numBlocks(void) const { return mDataBlocks.size(); }

  size_t totalBytes(void) const {
    size_t sum = 0;
    for (const auto& p : mDataBlocks) {
      sum += p.second.mSize;
    }
    return sum;
  }
};

template <typename AllocFactory = dagee::StdAllocatorFactory<> >
struct PartDeviceData : public PartHostData<AllocFactory> {
  using Base = PartHostData<AllocFactory>;
  using Base::mDataBlocks;

  BytePtr mDeviceDataPtr = nullptr;

  // use default constructor for Base because we'll fill in dataBlocks using offsets into
  // mDeviceDataPtr
  explicit PartDeviceData(const Base& dataTemplate) : Base() {
    auto totBytes = dataTemplate.totalBytes();

    BytePtr mDeviceDataPtr = nullptr;
    dagee::deviceAlloc(mDeviceDataPtr, totBytes);

    auto bytePtr = mDeviceDataPtr;
    for (auto i = dataTemplate.begin(), end_i = dataTemplate.end(); i != end_i; ++i) {
      this->addDataBlock(dataTemplate.varName(i), bytePtr, dataTemplate.blkSz(i));
      bytePtr += dataTemplate.blkSz(i);
    }
  }

  ~PartDeviceData(void) { dagee::deviceFree(mDeviceDataPtr); }

  // PartDeviceData(PartDeviceData&&) = delete;
  PartDeviceData(const PartDeviceData&) = delete;
  PartDeviceData& operator=(PartDeviceData&&) = delete;
  PartDeviceData& operator=(const PartDeviceData&) = delete;
};

template <typename DAGptr_tp, typename AllocFactory = dagee::StdAllocatorFactory<> >
class Partition {
 public:
  using DAGptr = DAGptr_tp;
  using HostData = PartHostData<AllocFactory>;
  using DeviceData = PartDeviceData<AllocFactory>;
  using HostDataPtr = HostData*;
  using DeviceDataPtr = DeviceData*;
  using DAGfillFunc = std::function<void(DAGptr, DeviceDataPtr)>;

 protected:
  DAGfillFunc mDAGfillFunc;
  HostDataPtr mHostData = nullptr;
  DeviceDataPtr mDeviceData = nullptr;
  DAGptr mDAGptr = nullptr;
  ATMItaskHandle mSrcTask;
  ATMItaskHandle mSinkTask;

  DAGptr& dagPtr(void) { return mDAGptr; }
  const DAGptr& dagPtr(void) const { return mDAGptr; }

 public:
  template <typename F>
  explicit Partition(F&& dagFillFunc, HostDataPtr hostData)
      : mDAGfillFunc(std::forward<F>(dagFillFunc)), mHostData(hostData) {}

  HostDataPtr& hostData(void) { return mHostData; }
  const HostDataPtr hostData(void) const { return mHostData; }

  DeviceDataPtr& deviceData(void) { return mDeviceData; }
  const DeviceDataPtr deviceData(void) const { return mDeviceData; }

  void fillDAG(DAGptr dag) {
    assert(dag && "null ptr arg");
    mDAGptr = dag;
    mDAGfillFunc(mDAGptr, mDeviceData);
  }

  DAGptr dag(void) const { return mDAGptr; }

  ATMItaskHandle& srcTask(void) { return mSrcTask; }
  const ATMItaskHandle& srcTask(void) const { return mSrcTask; }

  ATMItaskHandle& sinkTask(void) { return mSinkTask; }
  const ATMItaskHandle& sinkTask(void) const { return mSinkTask; }
};

} // end namespace dagee

#endif // DAGEE_INCLUDE_DAGEE_PARTITION_H
