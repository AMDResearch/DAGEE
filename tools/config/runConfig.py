# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/usr/bin/evn python3

from collections import namedtuple

DEFAULT_BUILD_DIR = '$HOME/testing/TaskingBenchmarks/build/'

""" Run class definiation
Class defines which benchmarks to run
with various run specific parameters
"""
Run = namedtuple('Run',
        field_names=['build_dir',
            'out_dir',
            'repetitions',
            'timeout',
            'env_vars',
            'env_var_tags',
            'benchmarks',
            'benchmark_tags',
            'inputs'])
# python 3.6 symantics
Run.__new__.__defaults__ = (DEFAULT_BUILD_DIR,
            DEFAULT_BUILD_DIR,
            3,
            1000,
            ['all'],
            ['default'],
            ['all'],
            ['all'],
            ['default'],
            )


run1 = Run()

run2 = Run(env_vars=['ATMI_DEVICE_GPU_WORKERS'], env_var_tags=['random'])
run3 = Run(env_vars=['ATMI_DEPENDENCY_SYNC_TYPE'], env_var_tags=['random'], repetitions=3)

run_list = [run3]

