# DAGEE (Directed Acyclic Graph Execution Engine)

## Prerequisites

0. Make sure following packages are installed: atmi, libelf-dev, libblas-dev, liblapack-dev, rocm-dkms, rocm-dev, rocminfo, comgr,
hip_base, hip_hcc, hsa-rocr-dev, hsa-ext-rocr-dev, rocm-smi, rocm-utils,
rocm-device-libs, rocm-opencl-dev, hsakmt-roct-dev 
1. Currently, DAGEE works with ROCm versions >= 2.0 and <= 3.3

## Building & Running

Clone TaskingBenchmarks repo and update the submodules

```
$ git clone <this-repo's-url>
$ git submodule update --init --recursive
```

Create a build directory:

```
$ mkdir build; cd build
```

Run CMake. 

```
$ CXX=/opt/rocm/bin/hipcc cmake ..
```
- CMake build options are as follows (Not required if defaults are OK):
  - ATMI_ROOT (default path /opt/rocm/atmi)
If these defaults are correct, you can omit the options when running cmake: 


Now you can compile the benchmarks

```
$ make -j
```

To run the benchmarks: 

```
  ./examples/kiteDagCpu
  ./examples/kiteDagGpu
```
