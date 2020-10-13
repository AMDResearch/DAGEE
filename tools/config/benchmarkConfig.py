# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/usr/bin/evn python3

# tvals = [8, 16, 32, 64, 128, 256]

# nw_args = {f'-s {s} -t {t}':[] for s in [16384, 32768] for t in tvals}

# lud_args = {f'-s {s} -t {t}':[] for s in [16384, 8192] for t in tvals}


gEnvVars = {
    'ATMI_DEVICE_GPU_WORKERS'  :{'full' : [1, 4, 8, 16, 24, 32],
                                 'default': [1],
                                 },
    'ATMI_DEPENDENCY_SYNC_TYPE':{'full' : [ 'ATMI_SYNC_CALLBACK',
                                           'ATMI_SYNC_BARRIER_PKT'],
                                 'default' : ['ATMI_SYNC_CALLBACK'],
                                 },
    'ATMI_MAX_HSA_SIGNALS' :{'full': [ 32, 64, 128, 256, 512, 1024, 2048, 4096 ],
                             'default': [1024],
                            }
}

benchmarks = {
    'NW' : {
        'wd': 'nw',
        'exe' : {'nw_atmi_task':{'tag':['ATMI', 'task'],
                                 'wd' : ''},
                'nw_hip_task':{'tag':['HIP', 'task'],
                                 'wd' : ''},
                'nw_atmi_bsp_cg':{'tag':['ATMI', 'bsp'],
                                 'wd' : ''},
                'nw_hip_bsp':{'tag':['HIP', 'BSP'],
                                 'wd' : ''},
                },
        'arg' : { '-s 16384 -t 32': {'tag':['large']},
                  '-s 1024 -t 32' : {'tag':['small', 'default']},
                  '-s 8192 -t 32' : {'tag':['medium']},
                },
        
        'project' : 'TaskingBenchmarks',
    },

    'LUD' : {
        'wd': 'lud',
        'exe' : {'lud_atmi_task' :{'tag': ['ATMI', 'task'] ,
                                   'wd' : ''},
                 'lud_atmi_bsp_cg' :{'tag': ['ATMI', 'bsp'] ,
                                     'wd' : ''},
                 'lud_hip_agg_task' :{'tag': ['HIP', 'task'],
                                      'wd' : ''},
                 'lud_hip_bsp' :{'tag': ['HIP', 'bsp'],
                                 'wd' : ''},
                 },
        'arg' : { '-s 1024 -t 32' : {'tag':['small']},
                  '-s 8192 -t 128': {'tag':['medium', 'default']},
                  '-s 16384 -t 512': {'tag':['large']},
                },
        'project' : 'TaskingBenchmarks',
    },

    'HIP_CHOL': {
        'wd': 'cholesky',
        'exe': {'bspCholeskyGPU' :{'tag': ['HIP', 'bsp'],
                                   'wd' : 'hipCholesky'},
                'taskCholeskyGPU':{'tag': ['HIP', 'task'],
                                   'wd' : 'hipCholesky'},
                'choleskyCPU':{'tag': ['CPU'],
                                   'wd' : 'hipCholesky'},
                },
            'arg': {'-s 4096 -a CholOpt::RIGHT': {'tag':['medium', 'default']},
               },

        'project' : 'TaskingBenchmarks',
    },

    'LINEAR_DAG' : {
        'wd': 'microbenchmarks',
        'exe' : { 'atmi-linearDAG': {'tag':['ATMI'],
                                     'wd' : 'ATMI/linear-dag'},
                 'hc-linearDAG':{'tag': ['HSA', 'HC'],
                                  'wd': 'HSA/linear-dag/host-callback'},
                 'bp-linearDAG':{'tag': ['HSA', 'BP'],
                                  'wd': 'HSA/linear-dag/barrier-pkt'},
                },
        'arg' : { '100 0' :{'tag':['no-work']}, 
                  '500 0' :{'tag':['no-work']},
                  '1000 0':{'tag':['default', 'no-work']},
                  '2000 0':{'tag':['no-work']},
                  '3000 0':{'tag':['no-work']}, 
                  '4000 0':{'tag':['no-work']},
                  '5000 0':{'tag':['no-work']} },

        'project' : 'TaskingBenchmarks',
    },

    'MULTI_CHILD_DAG' : {
        'wd': 'microbenchmarks',
        'exe' : { 'atmi-multi-child': {'tag':['ATMI'],
                                     'wd' : 'ATMI/multi-child'},
                  'single-bp':{'tag': ['HSA', 'BP', 'Single'],
                                  'wd': 'HSA/multi-child/barrier-pkt'},
                  'multi-bp':{'tag': ['HSA', 'BP', 'Multi'],
                                  'wd': 'HSA/multi-child/barrier-pkt'},
                },
        'arg' : { '100 0' :{'tag':['no-work']}, 
                  '500 0' :{'tag':['no-work']},
                  '1000 0':{'tag':['default', 'no-work']},
                  '2000 0':{'tag':['no-work']},
                  '3000 0':{'tag':['no-work']}, 
                  '4000 0':{'tag':['no-work']},
                  '5000 0':{'tag':['no-work']} },

        'project' : 'TaskingBenchmarks',
    },

    }
