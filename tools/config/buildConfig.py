# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#! /usr/bin/env python3

from build_helpers import Project
import os

BUILD_SYSTEM_DEFAULT = 'cmake'
BUILD_TYPE = 'RelWithDebInfo'

ROOT_DIR = os.path.join(os.getenv('HOME'),'testing')

Project.CMAKE_DICT = {'CMAKE_BUILD_TYPE': BUILD_TYPE}
Project.ROOT_DICT = {'root_dir' : ROOT_DIR}

ATMI = Project({
    'name'    : 'ATMI',
    'type'    : 'git',
    'address' : 'https://github.com/RadeonOpenCompute/atmi-staging.git',
    'branch_or_commit'  : 'master', # can be sha or branch
    'build'   : True,
    'flags'   : {'build' : {'HSA_DIR':'/opt/rocm',
                            'LLVM_DIR':'/opt/rocm/llvm',
                            'DEVICE_LIB_DIR':'/opt/rocm',
                            'ATMI_DEVICE_RUNTIME':'ON',},
                 'env' : {'LD_LIBRARY_PATH': 
                              '/opt/rocm/libhsakmt/lib:/opt/rocm/hsa/lib:/opt/rocm/lib',
                         }},
#    'root_dir': ROOT_DIR,
    'src_path' : 'src'
    })

print(ATMI._items)

"""
cppTasking = Project({
    'name'    : 'cppTasking',
    'type'    : 'git',
    'address' : 'git@gitlab1.amd.com:AsyncTasking/hipDAGGER.git',
    'branch_or_commit'  : 'master',
    'build'   : False,
    'flags'   : {'build' : {'HSA_ROOT': '/opt/rocm'},
                 'env' : {'LD_LIBRARY_PATH': 
                              '/opt/rocm/libhsakmt/lib:/opt/rocm/hsa/lib:/opt/rocm/lib'}
                 },
#    'root_dir' : ROOT_DIR,
    'deps'    : [('ATMI', ATMI, {'flags.build':'ATMI_ROOT', 
                                 'flags.env': 'LD_LIBRARY_PATH'})], 
    })

# currently using aguliani/build-automation branch as it has cmake handling the git 
# submodules at config time
print(cppTasking._items)
"""
TaskingBenchmarks = Project({ 
    'name'   : 'TaskingBenchmarks',
    'type'   : 'git',
    'address': 'git@gitlab1.amd.com:AsyncTasking/TaskingBenchmarks.git',
    'branch_or_commit' : 'master', # can be the 8 char abbrv-SHA or branch name 
    'build'  : True,
    'flags'  : {'build' : {'HSA_ROOT': '/opt/rocm/',
                           'AMD_LLVM': '/opt/rocm/llvm'},
                'env'   : {'CXX': '/opt/rocm/hip/bin/hipcc', 
                           'LD_LIBRARY_PATH': 
                              '/opt/rocm/libhsakmt/lib:/opt/rocm/hsa/lib:/opt/rocm/lib'}
               },
#    'root_dir' : ROOT_DIR,
    'deps'   :  [('ATMI', ATMI , {'flags.build':'ATMI_ROOT', 
                                  'flags.env': 'LD_LIBRARY_PATH'}),],
                 #('cppTasking', cppTasking, {'flags.build': 'CHAI_ROOT'})],
    })

def test1():
    """Simple build example function"""
    # I have split the fetch and build functions into two seprate calls
    ATMI.fetch() # clone the git repo into ${ROOT_DIR}/ATMI/src
    ATMI.build(sys=BUILD_SYSTEM_DEFAULT) # create the build dir and run make 
    # the built files will be resident inside ${ROOT_DIR}/ATMI/build

def test2():
    """ More involved example which checks dependencies and builds the TaskingBenchmarks repo"""
    # Step one checkdpendencies
    # this will fetch and build all dependencies
    if not TaskingBenchmarks.check_dependencies():
        print("Dependency check failed")
        return
    # Step2: Fetch the specified repo
    if not TaskingBenchmarks.fetch():
        print("Unable to fetch")
        return
    if not TaskingBenchmarks.build():
        print("Unable to build")

if __name__ == "__main__":
    # When run in this sequence ATMI will only be fetched once
#    test1()
#    test2()
    pass
