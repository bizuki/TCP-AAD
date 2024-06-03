import pandas as pd
import matplotlib.pyplot as plt
from itertools import count
import os

from .configuration import SuiteConfig, Model


MODEL_TO_FMT = {
    Model.VENO: 'o-k',
    Model.CUBIC: '.--k',
    Model.NEW_RENO: '^-k',
    Model.WESTWOOD_PLUS: 'v-k',
    Model.YEAH: '*--k',
    Model.BBR: 's-k',
    Model.CERL: 'D-k',
    Model.CERL_PLUS: 'P-k',
    Model.CERL_PLUS_X: 'X--k',
}

def create_share_plot(config: SuiteConfig, ds: pd.DataFrame):
    ds = _calculate_throughput(ds)
    ds.throughput /= 1024

    def create_df(model, name):
        new_ds = pd.DataFrame()
        new_ds = ds[ds.model == model][['throughput', 'ts']]
        new_ds = new_ds.rename(columns={'throughput': f'{name}'})
        return new_ds
    
    new_ds = pd.concat([create_df(i, f'{model.value}_{i}') for i, model in enumerate(config.models)])
    new_ds['ts'] = pd.to_timedelta(new_ds['ts'], unit='s')
    new_ds.set_index("ts", inplace=True)
    new_ds = new_ds.resample("100ms").aggregate(lambda x: x.sum() / 0.1)
    _create_dir('./plots')
    _create_dir(f'./plots/{config.name}')
    new_ds.plot().get_figure().savefig(f'./plots/{config.name}/fair_share.png')


def create_packets_plot(config: SuiteConfig, ds: pd.DataFrame):
    def create_df(model, name):
        new_ds = pd.DataFrame()
        new_ds = ds[ds.model == model][['packet_size', 'ts']]
        new_ds = new_ds.rename(columns={'packet_size': f'{name}'})
        new_ds[name] = new_ds[name].map(lambda _: 1)
        new_ds[name] = new_ds[name].cumsum()
        return new_ds
    
    new_ds = pd.concat([create_df(i, f'{model.value}_{i}') for i, model in enumerate(config.models)])
    new_ds['ts'] = pd.to_timedelta(new_ds['ts'], unit='s')
    new_ds.set_index("ts", inplace=True)
    new_ds = new_ds.resample("100ms").mean()
    _create_dir('./plots')
    _create_dir(f'./plots/{config.name}')
    new_ds.plot().get_figure().savefig(f'./plots/{config.name}/packets.png')


lst = 1
def _calculate_throughput(ds: pd.DataFrame):
    def add_throughput(row: pd.Series):
        global lst

        row['throughput'] = row['packet_size']
        lst = row['ts']
        return row
    
    new_ds_series = []
    
    for i in range(len(ds['model'].value_counts())):
        fil_ds = ds[ds['model'] == i].reset_index()
        new_ds_series.extend([add_throughput(row) for _, row in fil_ds.iterrows()])

    ds = pd.DataFrame(new_ds_series)
    ds = ds.drop('index', axis=1)

    return ds


def create_throughput_plot(config: SuiteConfig, stats: dict[Model, list[tuple[int, float]]], uplink: bool, data_rate: int):
    fig = plt.figure()

    new_plt = fig.add_subplot()
    for mdl, data in stats.items():
        new_plt.plot(*zip(*data), MODEL_TO_FMT[mdl], label=mdl.value)

    new_plt.set_ylabel('Average throughput (Mbps)')
    
    if config.loss_rate_congestion:
        new_plt.set_xlabel('Loss at L (Mbps)')
    if config.loss_rate_dist:
        new_plt.set_xlabel('Distance from AP (m)')
    if config.loss_rate_error:
        new_plt.set_xlabel('Loss at L (%)')
    if config.window_size:
        new_plt.set_xlabel('Window size of CerlPlusX (s)')
    if config.window_size_adapt:
        new_plt.set_xlabel('Window size of CerlPlusX (s)')
        new_plt.set_ylabel('Max time to adapt (s)')

    new_plt.xaxis.get_major_locator().set_params(integer=True)
    uplink_info = 'uplink' if uplink else 'downlink'
    new_plt.set_label(f'Throughput at data rate = {data_rate}Mbps and {uplink_info}')
    new_plt.legend()


    _create_dir('./plots')
    _create_dir(f'./plots/{config.name}')
    fig.savefig(f'./plots/{config.name}/throughput_{uplink_info}_{data_rate}.png')


def create_delay_plot(file: str):
    lines = open(file, 'r').readlines()
    sequences = {}

    acks = []
    data = []

    for line in lines:
        _, delay, seq, is_ack = map(float, line.split())
        
        if seq not in sequences:
            sequences[seq] = len(sequences)
        target = acks if is_ack else data
        target.append((sequences[seq], delay))
    

    fig = plt.figure()
    new_plt = fig.add_subplot()
    new_plt.scatter(*zip(*acks), color=['blue'], label='ack', marker='x', s=20)  
    new_plt.scatter(*zip(*data), color=['orange'], label='data', marker='+', s=20) 
    print(f'{len(acks)=}, {len(data)=}')
    new_plt.set_ybound(0, 100)
    new_plt.set_xbound(0, len(sequences))

    new_plt.legend()
    fig.savefig(f'./delay-with-adw.png')


def create_cmp_plot(file: str):
    lines = open(file, 'r').readlines()

    ts, iat, true_aggregation = zip(*map(lambda line: tuple(map(float, line.split())), lines))
    
    df = pd.DataFrame(
        data={
            'ts': ts,
            'iat': iat,
            'true_aggregation': true_aggregation
            # 'delay': delay,
            # 'smoothed_delay': smoothed_delay,
            # 'iat': iat,
            # 'cwnd': cwnd
        }
    )

    fig = plt.figure()
    new_plt = fig.add_subplot()
    
    new_plt.plot(ts, iat, label='_', c='k')
    
    changes = list(filter(lambda x: x[1] > x[2], zip(ts, true_aggregation, true_aggregation[1:])))

    for last, next, i in zip(changes, changes[1:], count()):
        if i % 2 == 0:
            plt.axvspan(last[0], next[0], facecolor='0.2', alpha=0.3)
        
    new_plt.set_xbound(7.30, 7.38)
    new_plt.set_ybound(0, 1)
    new_plt.set_title('IAT during aggregation')
    new_plt.set_ylabel('IAT (ms)')
    new_plt.set_xlabel('Timestamp (s)')

    fig.savefig(f'./figs/iat_aggregation.png')
    
        
def create_throughput_plot(stats: dict[str, dict[tuple, list[tuple[float, float]]]]):
    suites = list(stats.values())[0][0]

    for suite in suites:
        dr, mobility, fortyHz, tcp, udp, _, _, uplink = suite
        fig = plt.figure()
        current_stats = {name: stat[0][suite] for name, stat in stats.items()}

        new_plt = fig.add_subplot()

        for name, stat in current_stats.items():
            stat = list(map(lambda x: (x[0], x[1][0]), stat))
            new_plt.plot(*zip(*stat), label=name)

        new_plt.set_ylabel('Average throughput (Mbps)')
        new_plt.set_xlabel('Beta param')

        new_plt.set_title(f'Throughput at data rate = {dr}Mbps, Channel width = {40 if fortyHz else 20} Hz, Tcp nodes = {tcp}, Udp nodes = {udp}, Mobility = {mobility}, uplink={not uplink}', loc='center', wrap=True)
        new_plt.get_yaxis().get_major_formatter().set_useOffset(False)
        new_plt.legend()


        _create_dir('./plots_throughput')
        fig.savefig(f'./plots_throughput/throughput_{dr=}_{fortyHz=}_{tcp=}_{udp=}_{mobility=}_{uplink=}.png')

    for suite in suites:
        dr, mobility, fortyHz, tcp, udp, _, _, uplink = suite
        fig = plt.figure()
        current_stats = {name: stat[0][suite] for name, stat in stats.items()}

        new_plt = fig.add_subplot()


        for name, stat in current_stats.items():
            stat = list(map(lambda x: (x[0], x[1][1]), stat))
            new_plt.plot(*zip(*stat), label=name)

        new_plt.set_ylabel('Average End-to-End delay')
        new_plt.set_xlabel('Beta param')

        new_plt.set_title(f'Data rate = {dr}Mbps, Channel width = {40 if fortyHz else 20} Hz, Tcp nodes = {tcp}, Udp nodes = {udp}, Mobility = {mobility}, Uplink = {not uplink}', loc='center', wrap=True)
        new_plt.legend()

        _create_dir('./plots_delay')
        fig.savefig(f'./plots_delay/delay_{dr=}_{fortyHz=}_{tcp=}_{udp=}_{mobility=}_{uplink=}.png')


    suites = list(stats.values())[0][1]
    
    for suite in suites:
        dr, mobility, fortyHz, tcp, udp, tx, distance, uplink = suite
        fig = plt.figure()
        current_stats = {name: stat[1][suite] for name, stat in stats.items()}

        new_plt = fig.add_subplot()

        for name, stat in current_stats.items():
            stat = list(map(lambda x: (x[0], x[1][0]), stat))
            new_plt.plot(*zip(*stat), label=name)

        new_plt.set_ylabel('Average throughput (Mbps)')
        new_plt.set_xlabel('Beta param')

        new_plt.set_title(f'Throughput at data rate = {dr}Mbps, Channel width = {40 if fortyHz else 20} Hz, Tx={tx}, distance={distance}', loc='center', wrap=True)
        new_plt.get_yaxis().get_major_formatter().set_useOffset(False)
        new_plt.legend()


        _create_dir('./plots_tx')
        fig.savefig(f'./plots_tx/throughput_{tx=}_{distance=}.png')


def _create_dir(path: str):
    try:
        os.mkdir(path)
    except Exception:
        ...
