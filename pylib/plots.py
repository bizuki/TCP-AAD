import pandas as pd
import matplotlib.pyplot as plt
from collections import defaultdict
from pydantic import BaseModel
from typing import Callable
from itertools import count
import os
import numpy as np

from .configuration import SuiteConfig, Suite, DetailedStats


def create_cmp_plot(stats: list[DetailedStats]):    
    df = pd.concat(map(lambda st: st.dataframe, stats))
    fig = plt.figure()
    new_plt = fig.add_subplot()
    ts = df['ts'].to_list()
    iat = df['iat'].to_list()
    aggregation_pos = df['aggregation_position'].to_list()
    new_plt.plot(ts, iat, label='_', c='k')
    
    changes = list(filter(lambda x: x[1] > x[2], zip(ts, aggregation_pos, aggregation_pos[1:])))

    for last, next, i in zip(changes, changes[1:], count()):
        if i % 2 == 0:
            plt.axvspan(last[0], next[0], facecolor='0.2', alpha=0.3)
        
    new_plt.set_xbound(7.30, 7.38)
    new_plt.set_ybound(0, 1)
    new_plt.set_title('IAT of aggregation')
    new_plt.set_ylabel('IAT (ms)')
    new_plt.set_xlabel('Timestamp (s)')

    fig.savefig(f'./iat_aggregation.png')


def create_aggr_plot(stats: list[DetailedStats], beta: float):
    df = pd.concat(map(lambda st: st.dataframe, stats))
    fig = plt.figure()
    new_plt = fig.add_subplot()

    df.plot(x='ts', y='true_aggregation', ax=new_plt, label='True length of aggregation')
    df.plot(x='ts', y='currently_delayed', ax=new_plt, label='Estimation of length of aggregation')

    new_plt.set_title(f'$\\beta = {beta}$')

    new_plt.set_xlabel('Timestamp (s.)')
    new_plt.set_ylabel('Amount of packets')

    new_plt.set_xbound(4, 4.3)
    new_plt.set_ybound(0, df['currently_delayed'].max() + 10)

    fig.savefig(f'./aggr_beta_{beta}.png')

        
def create_throughput_plot(stats: dict[str, dict[tuple, list[tuple[float, float]]]]):
    suites = list(stats.values())[0][0]
    interesting_lambdas = [1.0, 2.0, 3.0]
    transformed_stats = {}

    algo_map = {
        'dack': 'TCP-ADW',
        'default': 'TCP-DD'
    }
    
    for suite in suites:
        current_stats = {name: stat[0][suite] for name, stat in stats.items()}
        for name, stat in current_stats.items():
            stat = list(map(lambda x: (x[0], x[1][0]), stat))
            if name == 'no-limit':
                [transformed_stats.update({(f'TCP-AAD $\\beta = {int(lam)}$', *suite): throughput}) for lam, throughput in stat if lam in interesting_lambdas]
            else:
                transformed_stats.update({(algo_map[name], *suite): stat[0][1]})

    bar_stats = {}
    
    for key, value in transformed_stats.items():
        name, dr, mobility, fortyHz, tcp, udp, _, _, uplink, _ = key

        bar_stats[dr, mobility, fortyHz, uplink] = bar_stats.get((dr, mobility, fortyHz, uplink), []) + [(f'{tcp} TCP, {udp} UDP', name, value)]

    for key, value in bar_stats.items():
        dr, mobility, fortyHz, uplink = key
        cases = list(sorted(set([case for case, algo, val in value])))
        algos = list(sorted(set([algo for case, algo, val in value])))
        sorted_val = list(sorted([val for case, algo, val in value]))
        
        X_axis = np.arange(len(cases)) 

        stats_ = {}
        for case, algo, val in value:
            if case not in stats_:
                stats_[case] = {}
            stats_[case][algo] = val

        algo_stats = []
        fig = plt.figure()
        new_plt = fig.add_subplot()

        for i, algo in enumerate(algos):
            algo_stats.append([])
            for case in cases:
                algo_stats[-1].append(stats_[case][algo])

            shift = i - (len(algos) - 1) / 2
            width = 0.1
        
            new_plt.bar(X_axis + shift * width, algo_stats[-1], width, label=algo)

        plt.xticks(X_axis, cases) 
        plt.xlabel("Cases") 
        plt.ylabel("Throughput (Mbps)") 
        plt.title(f"Throughput with Data Rate = {dr} Mbps")
        new_plt.legend(loc='upper right', title='Algorithms')
        new_plt.set_ybound(max(sorted_val[0] - 5, 0), sorted_val[-1] + 5)


        _create_dir('./plots_cases_udp')
        fig.savefig(f'./plots_cases/cases_udp_{dr=}_{fortyHz=}_{mobility=}_{uplink=}.png') 


    suites = list(stats.values())[0][0]


class PlotConfig(BaseModel):
    name: str
    dir: str
    title: str


class BarConfig(PlotConfig):
    group_by_columns: list[str]
    group_by_name: Callable[[tuple[float]], str]
    
    case_columns: list[str]
    case_name: Callable[[tuple[float]], str]

    group_title: str


def comparison_bars(
    data: dict[Suite, float | int], 
    config: BarConfig,
):
    assert len(set(config.group_by_columns).intersection(config.case_columns)) == 0, 'group by columns and case columns have the same column'
    
    stats: dict[tuple, float | int] = defaultdict(int)
    stats_count: dict[tuple, int] = defaultdict(int)
    
    # avg stats
    for suite, value in data.items():
        params = suite.get_params()
        key_params = tuple([params[k] for k in config.group_by_columns + config.case_columns])
        
        stats[key_params] = (stats[key_params] * stats_count[key_params] + value) / (stats_count[key_params] + 1)
        stats_count[key_params] += 1
        
    groups = list(sorted(set([params[:len(config.group_by_columns)] for params in stats])))
    cases = list(sorted(set([params[len(config.group_by_columns):] for params in stats])))
    sorted_val = list(sorted(stats.values()))
    
    X_axis = np.arange(len(cases)) 

    stats_ = {}
    for params, value in stats.items():
        group_params = params[:len(config.group_by_columns)]
        case_params = params[len(config.group_by_columns):]

        if case_params not in stats_:
            stats_[case_params] = {}
        stats_[case_params][group_params] = value

    group_stats = []
    fig = plt.figure()
    new_plt = fig.add_subplot()

    for i, group in enumerate(groups):
        group_stats.append([])
        for case in cases:
            group_stats[-1].append(stats_[case][group])

        shift = i - (len(groups) - 1) / 2
        width = 0.1
        new_plt.bar(X_axis + shift * width, group_stats[-1], width, label=config.group_by_name(group))

    plt.xticks(X_axis, map(config.case_name, cases)) 
    plt.xlabel("Cases") 
    plt.ylabel("Throughput (Mbps)") 
    plt.title(config.title)
    new_plt.legend(loc='upper right', title=config.group_title)
    new_plt.set_ybound(max(sorted_val[0] - 5, 0), sorted_val[-1] + 5) 
    _create_dir(f'./{config.dir}')
    fig.savefig(f'./{config.dir}/{config.name}.png') 
    

def _create_dir(path: str):
    try:
        os.mkdir(path)
    except Exception:
        ...
