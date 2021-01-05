# DAGEE (Directed Acyclic Graph Execution Engine)

DAGEE is a library for launching graphs of HIP Kernels, CPU functions and
data-movement operations. It currently works only on AMD Hardware supported by
ROCm.

## Prerequisites

1. Currently, DAGEE works with ROCm versions >= 2.0 and <= 3.3. We will start
   supporting latest ROCm versions in the near future.  
2. Make sure following packages are installed (in addition to a basic ROCm installation). All these are available from ROCm DEB or RPM repos:

  atmi
  comgr
  hcc
  hip-base
  hip-hcc
  hsa-ext-rocr-dev
  hsa-rocr-dev
  hsakmt-roct
  hsakmt-roct-dev
  rocm-cmake
  rocm-dev
  rocm-device-libs
  rocm-smi
  rocm-smi-lib64
  rocm-utils

3. Optional packages

  rocminfo
  rocprofiler-dev
  hsa-amd-aqlprofile
  rocm-opencl
  rocm-opencl-dev

## Building & Running

Clone this repository:

```
$ git clone <this-repo's-url>
$ git submodule update --init --recursive
```

Create a build directory:

```
$ mkdir build; cd build
```

Run CMake: 

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

## Documentation
We use Doxygen via `make doc` in the build directory to generate API documentation and a basic tutorial for DAGEE. 

## support
This is a research project undergoing development. Please open a Github issue if
you encounter a bug or a problem. 
