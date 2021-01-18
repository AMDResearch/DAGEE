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

3. Optional packages

  aomp-amdgpu
  rocminfo
  rocprofiler-dev
  hsa-amd-aqlprofile
  rocm-smi
  rocm-smi-lib64
  rocm-utils

## Building & Running

### Building ATMI
DAGEE builds on top of [ATMI](https://github.com/RadeonOpenCompute/atmi)

Clone ATMI:

```
git clone https://github.com/RadeonOpenCompute/atmi
```

Create ATMI build directory:

```
cd atmi
mkdir build
cd build

```

Run cmake:

```
cmake ../src
```

Run make:

```
make -j
```


### Building DAGEE

Clone this repository:

```
$ git clone <this-repo's-url>
```

Create a build directory:

```
$ mkdir build 
cd build
```

Run CMake: 

```
$ CXX=/opt/rocm/bin/hipcc cmake -DATMI_ROOT=<full-path-to-atmi-build-dir> ..
```

Now you can compile the benchmarks

```
$ make -j
```

To run the benchmarks: 

```
  ./examples/kiteDagCpu
  ./examples/kiteDagGpu
```

## Source Tree Organization
- `DAGEE-lib`   : the source code for DAGEE library
- `examples`    : some simple example code showing how DAGEE can be used
- `cppUtils`    : helper C++ classes and functions
- `tools`       : various scripts 
- `cmakeUtils`  : cmake utility functions and scripts 
- `doc`         : Source code for doxygen documentation

## Using DAGEE With your project

- Add `DAGEE-lib/include` and `cppUtils/include` to the compiler's include path
- Add ATMI_ROOT/include to the compiler's include path
- Link the executable with ATMI_BUILD/lib/libatmi_runtime.so


## Documentation
We use Doxygen. Run `make doc` in the build directory to generate API documentation and a basic tutorial for DAGEE in `<dagee-build-dir>/html`. 

## Support
This is a research project undergoing development. Please open a Github issue if
you encounter a bug or a problem. 
