# DAGEE (Directed Acyclic Graph Execution Engine)

## Prerequisites

0. Make sure following packages are installed: atmi, libelf-dev, libblas-dev, liblapack-dev, rocm-dkms, rocm-dev, rocminfo, comgr,
hip_base, hip_hcc, hsa-rocr-dev, hsa-ext-rocr-dev, rocm-smi, rocm-utils,
rocm-device-libs, rocm-opencl-dev, hsakmt-roct-dev 

## Building

1. Clone TaskingBenchmarks repo and update the submodules

```
$ git clone <this-repo's-url>
$ git submodule update --init --recursive
```

2. Create a build directory:

```
$ mkdir build; cd build
```

3. CMake build options are as follows (Not required if defaults are OK):
* ATMI_ROOT (default path /opt/rocm/atmi)

If these defaults are correct, you can omit the options when running cmake: 

```
$ CXX=/opt/rocm/bin/hipcc cmake ..
```

4. Now you can compile the benchmarks

```
$ make -j
```

5. To run the benchmarks: 

```
  ./examples/kiteDagCpu
  ./examples/kiteDagGpu
```
