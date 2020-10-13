# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#! /usr/bin/env python3

from pandas import DataFrame, read_csv
import matplotlib.pyplot as plt
import pandas as pd
import sys
import os

def makePlot(plotFile, inputFile):
    df = pd.read_csv(inputFile)

    #remove the row with dummy kernel if any
    if 'dummy' in df.iloc[0]['KernelName'].lower():
        df = df.drop([0])

    # normalize time-stamps by subtracting the first timestamp from all 
    offset = df.iloc[0]['DispatchNs']
    df['DispatchNs'] -= offset
    df['BeginNs'] -= offset
    df['EndNs'] -= offset
    df['CompleteNs'] -= offset

    fig = plt.figure()
    ax = plt.hlines(df.Index, df.BeginNs, df.EndNs)
    plt.xlabel('Time (ns)')
    plt.ylabel('Kernel ID')
    plt.savefig(plotFile, dpi=300, bbox_inches='tight', transparent=True)
    plt.show()

def main():
    if len(sys.argv) != 2:
        print("./extract-kernel-timeline.py <inputFile.csv>")

    # Read original CSV
    inputFile = sys.argv[1]
    bname = os.path.basename(inputFile)
    d = os.path.dirname(inputFile)
    prefix = os.path.splitext(bname)[0]
    plotFile = os.path.join(d, prefix + "-kernel-execution-timeline.pdf")

    # Make a plot
    makePlot(plotFile, inputFile)

if __name__ == '__main__':
    main()
