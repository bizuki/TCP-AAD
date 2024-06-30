import pandas as pd
import matplotlib.pyplot as plt
from collections import defaultdict
from pydantic import BaseModel
from typing import Callable
from itertools import count
import os
import numpy as np

from .configuration import SuiteConfig, Suite


# def create_throughput_plot(config: SuiteConfig, stats: dict[Model, list[tuple[int, float]]], uplink: bool, data_rate: int):
#     fig = plt.figure()

#     new_plt = fig.add_subplot()
#     for mdl, data in stats.items():
#         new_plt.plot(*zip(*data), MODEL_TO_FMT[mdl], label=mdl.value)

#     new_plt.set_ylabel('Average throughput (Mbps)')
    
#     if config.loss_rate_congestion:
#         new_plt.set_xlabel('Loss at L (Mbps)')
#     if config.loss_rate_dist:
#         new_plt.set_xlabel('Distance from AP (m)')
#     if config.loss_rate_error:
#         new_plt.set_xlabel('Loss at L (%)')
#     if config.window_size:
#         new_plt.set_xlabel('Window size of CerlPlusX (s)')
#     if config.window_size_adapt:
#         new_plt.set_xlabel('Window size of CerlPlusX (s)')
#         new_plt.set_ylabel('Max time to adapt (s)')

#     new_plt.xaxis.get_major_locator().set_params(integer=True)
#     uplink_info = 'uplink' if uplink else 'downlink'
#     new_plt.set_label(f'Throughput at data rate = {data_rate}Mbps and {uplink_info}')
#     new_plt.legend()


#     _create_dir('./plots')
#     _create_dir(f'./plots/{config.name}')
#     fig.savefig(f'./plots/{config.name}/throughput_{uplink_info}_{data_rate}.png')


# def create_delay_plot(file: str):
#     lines = open(file, 'r').readlines()
#     sequences = {}

#     acks = []
#     data = []

#     for line in lines:
#         _, delay, seq, is_ack = map(float, line.split())
        
#         if seq not in sequences:
#             sequences[seq] = len(sequences)
#         target = acks if is_ack else data
#         target.append((sequences[seq], delay))
    

#     fig = plt.figure()
#     new_plt = fig.add_subplot()
#     new_plt.scatter(*zip(*acks), color=['blue'], label='ack', marker='x', s=20)  
#     new_plt.scatter(*zip(*data), color=['orange'], label='data', marker='+', s=20) 
#     print(f'{len(acks)=}, {len(data)=}')
#     new_plt.set_ybound(0, 100)
#     new_plt.set_xbound(0, len(sequences))

#     new_plt.legend()
#     fig.savefig(f'./delay-with-adw.png')


# def create_cmp_plot(file: str):
#     lines = open(file, 'r').readlines()

#     ts, iat, true_aggregation = zip(*map(lambda line: tuple(map(float, line.split())), lines))
    
#     df = pd.DataFrame(
#         data={
#             'ts': ts,
#             'iat': iat,
#             'true_aggregation': true_aggregation
#             # 'delay': delay,
#             # 'smoothed_delay': smoothed_delay,
#             # 'iat': iat,
#             # 'cwnd': cwnd
#         }
#     )

#     fig = plt.figure()
#     new_plt = fig.add_subplot()
    
#     new_plt.plot(ts, iat, label='_', c='k')
    
#     changes = list(filter(lambda x: x[1] > x[2], zip(ts, true_aggregation, true_aggregation[1:])))

#     for last, next, i in zip(changes, changes[1:], count()):
#         if i % 2 == 0:
#             plt.axvspan(last[0], next[0], facecolor='0.2', alpha=0.3)
        
#     new_plt.set_xbound(7.30, 7.38)
#     new_plt.set_ybound(0, 1)
#     new_plt.set_title('IAT во время агрегации')
#     new_plt.set_ylabel('IAT (мс)')
#     new_plt.set_xlabel('Таймстамп (сек)')

#     fig.savefig(f'./iat_aggregation.png')


# def create_aggr_plot(file: str, beta: float):
#     lines = open(file, 'r').readlines()

#     ts, true_aggregation, currently_delayed = zip(*map(lambda line: tuple(map(float, line.split())), lines))

#     ts, true_aggregation, currently_delayed = zip(*[t for t in zip(ts, true_aggregation, currently_delayed) if 4.3 >= t[0] >= 4])
    
#     df =pd.DataFrame(
#         data={
#             'ts': ts,
#             'currently_delayed': currently_delayed,
#             'true_aggregation': true_aggregation
#             # 'delay': delay,
#             # 'smoothed_delay': smoothed_delay,
#             # 'iat': iat,
#             # 'cwnd': cwnd
            
#         }
#     )


#     fig = plt.figure()
#     new_plt = fig.add_subplot()

#     df.plot(x='ts', y='true_aggregation', ax=new_plt, label='настоящая длина агрегации')
#     df.plot(x='ts', y='currently_delayed', ax=new_plt, label='оценочная длина агрегации')

#     new_plt.set_title(f'$\\beta = {beta}$')

#     new_plt.set_xlabel('Таймстамп (сек.)')
#     new_plt.set_ylabel('Количество пакетов')

#     new_plt.set_xbound(4, 4.3)
#     new_plt.set_ybound(0, df['currently_delayed'].max() + 10)

#     fig.savefig(f'./aggr_beta_{beta}.png')

        
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
    #     with open(f'./plots_cases/cases_{dr=}_{fortyHz=}_{mobility=}_{uplink=}.res', 'w') as f:
    #         for algo in algos:
    #             f.write(f'{algo}\t')
    #         f.write('\n')
    #         for case in cases:
    #             f.write(f'case: {case}: \t')
    #             for algo in algos:
    #                 f.write(str(100 * (stats_[case][algo] / stats_[case]['TCP-DD']) - 100) + '%')
    #                 f.write('\t')
    #             f.write('\n')


    suites = list(stats.values())[0][0]

    # for suite in suites:
    #     dr, mobility, fortyHz, tcp, udp, _, _, uplink = suite
    #     fig = plt.figure()
    #     current_stats = {name: stat[0][suite] for name, stat in stats.items()}

    #     new_plt = fig.add_subplot()

    #     for name, stat in current_stats.items():
    #         stat = list(map(lambda x: (x[0], x[1][0]), stat))
    #         new_plt.plot(*zip(*stat), label=name)

    #     new_plt.set_ylabel('Average throughput (Mbps)')
    #     new_plt.set_xlabel('Beta param')

    #     new_plt.set_title(f'Throughput at data rate = {dr}Mbps, Channel width = {40 if fortyHz else 20} Hz, Tcp nodes = {tcp}, Udp nodes = {udp}, Mobility = {mobility}, uplink={not uplink}', loc='center', wrap=True)
    #     new_plt.get_yaxis().get_major_formatter().set_useOffset(False)
    #     new_plt.legend()


    #     _create_dir('./plots_throughput')
    #     fig.savefig(f'./plots_throughput/throughput_{dr=}_{fortyHz=}_{tcp=}_{udp=}_{mobility=}_{uplink=}.png')

    # for suite in suites:
    #     dr, mobility, fortyHz, tcp, udp, _, _, uplink = suite
    #     fig = plt.figure()
    #     current_stats = {name: stat[0][suite] for name, stat in stats.items()}

    #     new_plt = fig.add_subplot()


    #     for name, stat in current_stats.items():
    #         stat = list(map(lambda x: (x[0], x[1][1]), stat))
    #         new_plt.plot(*zip(*stat), label=name)

    #     new_plt.set_ylabel('Average End-to-End delay')
    #     new_plt.set_xlabel('Beta param')

    #     new_plt.set_title(f'Data rate = {dr}Mbps, Channel width = {40 if fortyHz else 20} Hz, Tcp nodes = {tcp}, Udp nodes = {udp}, Mobility = {mobility}, Uplink = {not uplink}', loc='center', wrap=True)
    #     new_plt.legend()

    #     _create_dir('./plots_delay')
    #     fig.savefig(f'./plots_delay/delay_{dr=}_{fortyHz=}_{tcp=}_{udp=}_{mobility=}_{uplink=}.png')


    # suites = list(stats.values())[0][1]
    # transformed_stats = {}

    # for suite in suites:
    #     current_stats = {name: stat[1][suite] for name, stat in stats.items()}
    #     for name, stat in current_stats.items():
    #         stat = list(map(lambda x: (x[0], x[1][0]), stat))
    #         if name == 'no-limit':
    #             [transformed_stats.update({(f'TCP-AAD $\\beta = {int(lam)}$', *suite): throughput}) for lam, throughput in stat if lam in interesting_lambdas]
    #         else:
    #             transformed_stats.update({(algo_map[name], *suite): stat[0][1]})

    # dist_stats = {}
    # for key, value in transformed_stats.items():
    #     name, dr, mobility, _, _, _, _, dist, _ = key
    #     if name not in dist_stats:
    #         dist_stats[name] = []
    #     dist_stats[name].append((100 * dist / 160, value))
    
    # fig = plt.figure()
    # new_plt = fig.add_subplot()
    # for name, stats in dist_stats.items():
    #     new_plt.plot(*zip(*stats), label=name)
    
    # new_plt.set_ylabel('Average throughput (Mbps)')
    # new_plt.set_xlabel('Distance from AP (m)')
    # new_plt.legend()

    # _create_dir('./plots_dist')
    # fig.savefig(f'./plots_dist/cmp.png')

    # new_plt.set_xbound(89, 95)
    # new_plt.set_ybound(0, 20)
    # fig.savefig(f'./plots_dist/cmp_close.png')
    
    # for suite in suites:
    #     dr, mobility, fortyHz, tcp, udp, tx, distance, uplink = suite
    #     fig = plt.figure()
    #     current_stats = {name: stat[1][suite] for name, stat in stats.items()}

    #     new_plt = fig.add_subplot()

    #     for name, stat in current_stats.items():
    #         stat = list(map(lambda x: (x[0], x[1][0]), stat))
    #         new_plt.plot(*zip(*stat), label=name)

    #     new_plt.set_ylabel('Average throughput (Mbps)')
    #     new_plt.set_xlabel('Beta param')

    #     new_plt.set_title(f'Throughput at data rate = {dr}Mbps, Channel width = {40 if fortyHz else 20} Hz, Tx={tx}, distance={distance}', loc='center', wrap=True)
    #     new_plt.get_yaxis().get_major_formatter().set_useOffset(False)
    #     new_plt.legend()


    #     _create_dir('./plots_tx')
    #     fig.savefig(f'./plots_tx/throughput_{tx=}_{distance=}.png')

    # suites = list(stats.values())[0][2]

    # for suite in suites:
    #     current_stats = {name: stat[2][suite] for name, stat in stats.items()}
    #     for name, stat in current_stats.items():
    #         stat = list(map(lambda x: (x[0], x[1][0]), stat))
    #         if name == 'no-limit':
    #             [transformed_stats.update({(f'TCP-AAD $\\beta = {int(lam)}$', *suite): throughput}) for lam, throughput in stat if lam in interesting_lambdas]
    #         else:
    #             transformed_stats.update({(algo_map[name], *suite): stat[0][1]})

    # bar_stats = {}
    
    # for key, value in transformed_stats.items():
    #     name, dr, mobility, fortyHz, tcp, udp, _, _, uplink, tcpVariant = key

    #     bar_stats[dr, mobility, fortyHz, uplink] = bar_stats.get((dr, mobility, fortyHz, uplink), []) + [(tcpVariant, name, value)]

    # for key, value in bar_stats.items():
    #     dr, mobility, fortyHz, uplink = key
    #     cases = list(sorted(set([case for case, algo, val in value])))
    #     algos = list(sorted(set([algo for case, algo, val in value])))
    #     sorted_val = list(sorted([val for case, algo, val in value]))
        
    #     X_axis = np.arange(len(cases)) 

    #     stats_ = {}
    #     for case, algo, val in value:
    #         if case not in stats_:
    #             stats_[case] = {}
    #         stats_[case][algo] = val

    #     algo_stats = []
    #     fig = plt.figure()
    #     new_plt = fig.add_subplot()

    #     for i, algo in enumerate(algos):
    #         algo_stats.append([])
    #         for case in cases:
    #             algo_stats[-1].append(stats_[case][algo])

    #         shift = i - (len(algos) - 1) / 2
    #         width = 0.1
        
    #         new_plt.bar(X_axis + shift * width, algo_stats[-1], width, label=algo)

    #     plt.xticks(X_axis, cases) 
    #     plt.xlabel("TCP congestion control mechanisms") 
    #     plt.ylabel("Throughput (Mbps)") 
    #     plt.title(f"Throughput with Data Rate = {dr} Mbps")
    #     new_plt.legend(loc='upper right', title='Algorithms')
    #     new_plt.set_ybound(max(sorted_val[0] - 5, 0), sorted_val[-1] + 5)


    #     _create_dir('./plots_variants')
    #     fig.savefig(f'./plots_variants/variants_{dr=}_{fortyHz=}_{mobility=}_{uplink=}.png') 
    #     with open(f'./plots_variants/variants_{dr=}_{fortyHz=}_{mobility=}_{uplink=}.res', 'w') as f:
    #         for algo in algos:
    #             f.write(f'{algo}\t')
    #         f.write('\n')
    #         for case in cases:
    #             f.write(f'case: {case}: \t')
    #             for algo in algos:
    #                 f.write(str(100 * (stats_[case][algo] / stats_[case]['TCP-DD']) - 100) + '%')
    #                 f.write('\t')
    #             f.write('\n')


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
