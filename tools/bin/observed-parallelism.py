# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#! /usr/bin/env python3
from pandas import DataFrame, read_csv
import matplotlib.pyplot as plt
import matplotlib.backends.backend_pdf as pdf
import pandas as pd
import sys
import os

def measureObservedParallelism(inputFile, outputFile):
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

    numRows = df.shape[0]

    concurrency = []
    for idx, row in df.iterrows():
        # if(idx == 0 and 'dummy' in row['KernelName']):
            # continue
        beg = row['BeginNs']
        end = row['EndNs']

        observedParallelism = 1
        # print(f'Begin = {beg}, End = {end}')
        for j in range(idx, numRows):
            # print(f"{j}, {df.iloc[j]['BeginNs']}")
            if df.iloc[j]['BeginNs'] < end:
                observedParallelism += 1
            else:
                break

        concurrency.append(observedParallelism)

    df['DispatchToBeginNs'] = df['BeginNs'] - df['DispatchNs']
    df['EndToCompleteNs'] = df['CompleteNs'] - df['EndNs']
    df['ConcurrentKernels'] = concurrency
    df.to_csv(outputFile)
    return df

def timelinePlotHelper(df, of, yvar, title):
    ptsz = 0.3
    fig = plt.figure()
    ax = df.plot.scatter('BeginNs', yvar, s=ptsz)
    # print(ax)
    plt.title(title)
    plt.xlabel('Timeline (Ns)')
    plt.ylabel(yvar)
    plt.axhline(y=df[yvar].mean(), color='red')
    of.savefig(ax.figure, bbox_inches='tight')

def histPlotHelper(df, of, var, title):
    fig = plt.figure()
    ax = df[var].hist()
    plt.title(title)
    of.savefig(ax.figure, bbox_inches='tight')


def makePlot(df, plotFile):
    of = pdf.PdfPages(plotFile)
    ptsz = 0.3

    timelinePlotHelper(df, of, 'ConcurrentKernels',  'Kernel Concurrency vs Timeline')
    timelinePlotHelper(df, of, 'DurationNs', 'Kernel Duration vs Timeline')
    timelinePlotHelper(df, of, 'DispatchToBeginNs', 'Kernel "Dispatch to Begin Delay" vs Timeline')
    timelinePlotHelper(df, of, 'EndToCompleteNs', 'Kernel "End to Completion Delay" vs Timeline')

    # histPlotHelper(df, of, 'ConcurrentKernels', 'Kernel Concurrency Histogram');
    # histPlotHelper(df, of, 'DurationNs', 'Kernel Duration Histogram');
    # histPlotHelper(df, of, 'DispatchToBeginNs', 'Kernel DispatchToBeginNs Histogram');
    # histPlotHelper(df, of, 'EndToCompleteNs', 'Kernel EndToCompleteNs Histogram');

    of.close()
    # plt.show()

def main():

    if len(sys.argv) != 2:
        print("./extract-kernel-timeline.py <inputFile.csv>")

    # Read original CSV
    inputFile = sys.argv[1]
    bname = os.path.basename(inputFile)
    d = os.path.dirname(inputFile)
    prefix = os.path.splitext(bname)[0]
    outputFile = os.path.join(d, prefix + "-processed.csv")
    plotFile = os.path.join(d, prefix + "-plots.pdf")
    df = measureObservedParallelism(inputFile, outputFile)
    # print(df['ConcurrentKernels'])

    # Make a plot
    makePlot(df=df, plotFile=plotFile)

if __name__ == '__main__':
    main()
