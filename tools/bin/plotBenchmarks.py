# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#! /bin/python

# Usage plotting.py <csv file>


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
import seaborn as sns
plt.style.use('ggplot')


def plot_best_fig(all_data, needed, save_fig=False, save_type='png'):
    apps = list(all_data['app'].unique())
    for app in apps:
        if app == "LINEAR_DAG":
            continue
        __sel = all_data[all_data['app'] == app]
        args = list(__sel['arg'].unique())
        for arg in args:
            __sel_arg = __sel[__sel['arg'] == arg]
            exes = list(__sel_arg['exe'].unique())
            depend_type = None
            for exe in exes:
                if 'task' in exe:
                    depend_type = __sel_arg.loc[__sel_arg[__sel_arg['exe']
                                                          == exe][needed[-6]].idxmin()][needed[:2]]
            __best = __sel_arg[(__sel_arg[needed[0]] == depend_type[needed[0]]) & (
                __sel_arg[needed[1]] == depend_type[needed[1]])]
            graph = sns.factorplot(hue="Type",
                                   y=needed[-6],
                                   order=['Tasking',
                                          'BSP'],
                                   hue_order=["Tasking",
                                              "BSP"],
                                   x="Type",
                                   data=__best,
                                   kind='bar')
            plt.title(app + ": " + arg)
            if save_fig:
                graph.savefig(
                    fname=app +
                    arg +
                    "_best." +
                    save_type,
                    transparent=True,
                    dpi=600,
                    format=save_type,
                    bbox_inches='tight')


def plot_sweep_fig(all_data, needed, save_fig=False, save_type='png'):
    apps = list(all_data['app'].unique())
    for app in apps:
        __sel = all_data[all_data['app'] == app]
        exes = list(__sel['exe'].unique()).sort()
        args = list(__sel['arg'].unique())
        print(app)
        for x_arg in [0, 1]:
            if app == "LINEAR_DAG":
                __sel['Per-Kernel Time (ms)'] = __sel[needed[3]]
                for arg in args:
                    val = int(arg.strip().split()[0])
                    __sel.loc[(__sel['arg'] == arg), 'Per-Kernel Time (ms)'] = __sel[__sel['arg']
                                                                                     == arg]['Per-Kernel Time (ms)'] / val
                graph = sns.factorplot(
                    hue=needed[0],
                    y='Per-Kernel Time (ms)',
                    order=exes,
                    x=needed[1],
                    col="arg",
                    data=__sel,
                    kind='bar')
                if save_fig:
                    graph.savefig(
                        fname=app +
                        "_sweep." +
                        save_type,
                        transparent=True,
                        frameon=False,
                        dpi=600,
                        format=save_type,
                        bbox_inches='tight')
                # plt.title(app)
            else:
                graph = sns.factorplot(hue="Type",
                                       y=needed[-6],
                                       hue_order=["Tasking",
                                                  "BSP"],
                                       x=needed[x_arg],
                                       col="arg",
                                       data=__sel,
                                       kind='bar')
                if save_fig:
                    graph.savefig(
                        fname=app +
                        "_" +
                        needed[x_arg] +
                        "_sweep." +
                        save_type,
                        transparent=True,
                        frameon=False,
                        dpi=600,
                        format=save_type,
                        bbox_inches='tight')
                # plt.title(app)


def get_df_from_file(input_file_name):
    return pd.read_csv(input_file_name)


def main_plot_fn(input_file_name=None, input_df=None,
                 plot_sweeps=True, save_out=False):
    """
    Main Plotting function: Takes an input file name or an input df and produces the plots
    """
    data = None
    if input_file_name is not None:
        data = get_df_from_file(input_file_name)
    elif input_df is not None:
        data = input_df
    else:
        print("Error: No Input file ir DF given")
        return

    needed = ['ATMI_DEPENDENCY_SYNC_TYPE', 'ATMI_MAX_HSA_SIGNALS', 'DAG Creation Time (ms)',
              'DAG Execution Time (ms)', 'Lower-Right Matrix Time (ms)', 'Matrix Size',
              'Num Tasks Launched', 'Task Size',
              'Task Size (block)', 'Tile Size', 'Total Time (ms)', 'Upper-left Matrix Time (ms)',
              'app', 'arg', 'exe', 'repitition']

    all_data = data[needed]

    def replace_name(x):
        if 'task' in x:
            return "Tasking"
        elif 'bsp' in x:
            return "BSP"
        else:
            return x
    all_data['Type'] = data[needed[-2]]
    all_data['Type'] = all_data['Type'].apply(replace_name)

    plot_best_fig(all_data, needed, save_out)
    if plot_sweeps:
        plot_sweep_fig(all_data, needed, save_out)


if __name__ == "__main__":
    import sys
    sns.set(context="talk", palette="Paired")
    main_plot_fn(sys.argv[1], save_out=True, plot_sweeps=True)
