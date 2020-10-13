# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/usr/bin/env python3 

import pandas as pd
import numpy as np
import sys

totalTime = 'Total Time (ms)'

def replace_name(x):
    if 'task' in x:
        return "Tasking"
    elif 'bsp' in x:
        return "BSP"
    else:
        return x

def findLang(x):
    if 'hip' in x:
        return 'hip'
    elif 'atmi' in x:
        return 'atmi'
    else:
        return x

df = pd.read_csv(sys.argv[1])

df['type'] = df['exe'].apply(replace_name)
df['lang'] = df['exe'].apply(findLang)

apps = list(df['app'].unique())
depModes = list(df['ATMI_DEPENDENCY_SYNC_TYPE'].unique())

bestTimes = pd.DataFrame()

for dep in depModes: 
    for app in apps:
        for l in list(df['lang'].unique()):
            filt = (df['ATMI_DEPENDENCY_SYNC_TYPE'] == dep) & (df['app'] == app) & (df['lang'] == l)

            tasking = df[ filt & (df['type'] == 'Tasking')]
            bsp = df[filt & (df['type'] == 'BSP')]

            if not tasking.empty:
                bt = tasking.loc[tasking[totalTime].idxmin()]
                bestTimes = bestTimes.append(bt)

            if not bsp.empty:
                bb = bsp.loc[bsp[totalTime].idxmin()]
                bestTimes = bestTimes.append(bb)



    # print(bsp)
    # print('#####################')
    
cols=['ATMI_DEPENDENCY_SYNC_TYPE',
        'ATMI_MAX_HSA_SIGNALS', 'app', 'exe', 'arg', totalTime, 'type', 'lang']
print(bestTimes[cols])




