# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/usr/bin/env python3

import re
import itertools
import os
import sys
import subprocess
import datetime
import numpy as np
import pandas as pd


import benchmarkConfig as bc
import runConfig as rc

ENABLE_DEBUG=False
DRY_RUN=False

def dbgPrint(*args):
    if ENABLE_DEBUG:
        print(*args)

def shellInterpretStr(s):
    """ Convert shell strings to complete expanded paths """
    return os.path.expanduser(os.path.expandvars(s))

def expand_env_vals(in_dict, sweep_keys, tags):
    """ Expand environment variables """
    exp_dict = []
    for (k, v) in in_dict.items():
        if k in sweep_keys:
            # sweep key use tag to get value
            dbgPrint(k)
            valid_tags = None
            valid_tags = [tag if tag in v.keys() else 'full' for tag in tags]
            dbgPrint(valid_tags)
            exp_dict.append([{ 'type': 'env', 'name' : k , 'val' : i } for tag in valid_tags for i in v[tag]])
        else:
            # get default value for the rest
            exp_dict.append([{ 'type': 'env', 'name' : k , 'val' : i } for i in v['default']])
    # return expanded dictionary
    # dbgPrint(exp_dict)
    return exp_dict

def get_tagged_list(in_dict, tags):
    """ Get items from the main benchmark list with tagged values """
    if 'all' in tags:
        # return all keys 
        return [k for k,v in in_dict.items()]
    else:
        # return keys with any of the tags
        return [k for k,v in in_dict.items() if any(tag in v['tag'] for tag in tags)]

for curr_run in rc.run_list:
    # 1. get the env-variables (from first list) 
    #    and there sweep values (use tags as keys)
    dbgPrint(curr_run)
    envLists = None
    if 'all' in curr_run.env_vars:
        # sweep all params
        envLists = expand_env_vals(bc.gEnvVars, bc.gEnvVars.keys(), curr_run.env_var_tags)
    else:
        # sweep selected params
        envLists = expand_env_vals(bc.gEnvVars, curr_run.env_vars, curr_run.env_var_tags)

    # create product space
    envVarProd = [p for p in itertools.product(*envLists)]

    if ENABLE_DEBUG:
        for p in envVarProd:
            dbgPrint(p)
    dbgPrint('#####################')

    # getting the benchmark list
    ## we have a list of benchmarks that is present in curr_run.benchmarks
    ## we have tags for which exes we want t o run in curr_run.bench_tags
    ### input tags to use are present in curr_run.inputs
    # filter if not 'all' in tags and benchmarks
    # then select inputs 
    run_bench = None
    if 'all' in curr_run.benchmarks:
        # run all benchmarks
        run_bench = bc.benchmarks
    else:
        # filter the the benchmark list
        run_bench = {k:bc.benchmarks[k] for k in curr_run.benchmarks} 

    benchList = [{ 'type' : 'prog', 'name': k, 'wd': v['wd'], 'exe': e, 'arg': a } 
                        for k, v in run_bench.items() 
                        for e, a in itertools.product(get_tagged_list(v['exe'], curr_run.benchmark_tags), 
                                                      get_tagged_list(v['arg'], curr_run.inputs))]
            
    if ENABLE_DEBUG:
        for b in benchList:
            dbgPrint(b)
    dbgPrint('#####################')

    env_exe_list = [{ 'env': e, 'prog': p } for e,p in itertools.product(envVarProd, benchList)]

    if ENABLE_DEBUG:
        for r in env_exe_list:
            dbgPrint(r)
    dbgPrint('#####################')

    # convert to command line

    # TODO: Fix CPU freq and GPU frq to max/constant value 
    # FIXME: use cmdline args to set output file and some other properties
    timestamp = datetime.datetime.now().strftime('%Y-%m-%d--%H.%M.%S')
    outdir = shellInterpretStr(f'{curr_run.out_dir}')
    runFile = f'{outdir}/run-at-{timestamp}.log'
    csvFile = f'{outdir}/run-summary-{timestamp}.csv'

    ofh = open(runFile, 'w')

    ofh.write(f'RUN INFO: {str(curr_run)}\n')

    allData = pd.DataFrame()

    ENABLE_DEBUG = True# TODO: remove

    counter = 0
    for r in env_exe_list:
        prog = r['prog']
        env  = r['env']

        wd = shellInterpretStr(f"{curr_run.build_dir}/{prog['wd']}")

        envMap = {e['name']: str(e['val']) for e in env }

        cmdStr = f"./{prog['exe']} {prog['arg']}"
        dbgPrint(cmdStr)

        runningCmd = 'Running ' + str(envMap) + ' ' + cmdStr + '\n'
        print(runningCmd)

        appName = prog['name']

        currData = {}

        if not DRY_RUN:
            for i in range(curr_run.repetitions):
                res = subprocess.run(cmdStr, env=envMap, shell=True, cwd=wd, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=curr_run.timeout)

                ofh.write('BEGIN_RUN ===========================================================\n')
                ofh.write(runningCmd)

                currData['app'] = appName
                currData['exe'] = prog['exe']
                currData['arg'] = prog['arg']
                currData['repitition'] = i
                currData['tags'] = "/".join(bc.benchmarks[appName]['exe'][prog['exe']]['tag'])
                currData['timestamp'] = timestamp 

                for k, v in currData.items():
                    ofh.write(f"PARAM, {appName}, {k}, {v}\n")

                for (k,v) in envMap.items():
                    ofh.write(f"PARAM, {appName}, {k}, {v}\n")
                    currData[k] = v

                outputText = res.stdout.decode(ofh.encoding)
                ofh.write(outputText)
                ofh.write('END_RUN =============================================================\n')
                ofh.flush()

                # Inline log parser
                ptrn = re.compile(r'^PARAM|^STAT')
                for line in outputText.split('\n'):
                    if ptrn.match(line):
                        arr = line.split(',')
                        topic = arr[1].strip()
                        statOrParam = arr[2].strip()
                        value = arr[3].strip()
                        dbgPrint(','.join([topic, statOrParam, value]))
                        currData[statOrParam] = value
                        
                df = pd.DataFrame(currData, index=[counter])
                counter += 1
                dbgPrint(df)

                # allData = allData.append(df, sort=True)
                # XXX: sort=True works 0.23 onwards
                allData = allData.append(df)
            # end for
            print(allData)
            allData.to_csv(csvFile, index=False)
