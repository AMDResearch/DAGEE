// Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

// This is not intended for compilation. Just for thinking through the interface
int main() {

  std::vector<PartData> hostData;

  gt::OutOfCoreATMIexecutor ex;
  ex.addKernel<types...>(kernelFunc0);
  ex.addKernel<types...>(kernelFunc1);

  ex.makeDeviceDataBuffers(hostData[0], numBuffers); // based on some budget

  auto partDAG = ex.makePartitionDAG(dagPtr);

  for (i = 0; i < numPartitions; ++i) {
    auto dagPtr = ex.makeDAG();
    partDAG->addNode(Partition(dagPtr, &hostData[i]));
  }
  while(someCond) {
    partDAG->addEdge(srcPart, dstPart);
  }
  
  ex.prepareDAG(partDAG);

  ex.executeDAG(partDAG);



}
