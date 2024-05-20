import pandas as pd
import matplotlib.pyplot as plt
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

    ts, delay, smoothed_delay, iat, cwnd = zip(*map(lambda line: tuple(map(float, line.split())), lines))
    
    df =pd.DataFrame(
        data={
            'ts': ts,
            'delay': delay,
            'smoothed_delay': smoothed_delay,
            'iat': iat,
            'cwnd': cwnd
        }
    )

    fig = plt.figure()
    new_plt = fig.add_subplot()
    
    df.plot(x='ts', y='iat', ax=new_plt)
    df.plot(x='ts', y='cwnd', ax=new_plt, secondary_y=True)

    fig.savefig(f'./cmp_iat.png')
    
        
def create_throughput_plot(stats: dict[str, list[list[tuple[float, float]], float]]):

    for i, dr in enumerate([40, 70]):
        fig = plt.figure()
        current_stats = {name: stat[i] for name, stat in stats.items()}
        print(current_stats)

        new_plt = fig.add_subplot()

        for name, stat in current_stats.items():

            new_plt.plot(*zip(*stat), label=name)

        new_plt.set_ylabel('Average throughput (%)')
        new_plt.set_xlabel('Lambda param')

        new_plt.set_label(f'Throughput at data rate = {dr}Mbps')
        new_plt.legend()

        _create_dir('./plots')
        fig.savefig(f'./plots/throughput_{dr}.png')


def _create_dir(path: str):
    try:
        os.mkdir(path)
    except Exception:
        ...
