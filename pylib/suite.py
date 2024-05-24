import os
import pandas as pd
import numpy as np
from .configuration import SuiteConfig, Model 
from .plots import create_throughput_plot

def process_suite(
    config: SuiteConfig
):    
    if config.loss_rate_dist:
        for uplink in [True]:
            print(f'processing {uplink=}')
            for data_rate in range(1, 7, 2):
                print(f'processing {uplink=}, {data_rate=}')

                res = {}
                for mdl in config.models:
                    new_params = dict(tcpVariant=mdl.value, nodes=10, uplink=uplink, dataRate=data_rate)
                    print(f'{mdl} is processing with {data_rate=} and {uplink=}')
                    throughput = collect_packet_loss_stats_dist(new_params)
                    print(f'{mdl} is processed with {data_rate=} and {uplink=}')
                    res[mdl] = throughput
                
                    with open(f'dist_{data_rate}.res', 'w') as f:
                        f.write(f'{res}')

                create_throughput_plot(config, res, uplink, data_rate)
                print(f'processed {uplink=}, {data_rate=}')
        print(f'processed {uplink=}')

    if config.loss_rate_error:
        for uplink in [True]:
            print(f'processing {uplink=}')
            for data_rate in range(1, 4, 2):
                print(f'processing {uplink=}, {data_rate=}')

                res = {}
                for mdl in config.models:
                    new_params = dict(tcpVariant=mdl.value, nodes=10, uplink=uplink, dataRate=data_rate)
                    print(f'{mdl} is processing with {data_rate=} and {uplink=}')
                    throughput = collect_packet_loss_stats_error(new_params)
                    print(f'{mdl} is processed with {data_rate=} and {uplink=}')
                    res[mdl] = throughput

                with open(f'error_{data_rate}.res', 'w') as f:
                    f.write(f'{res}')
                
                create_throughput_plot(config, res, uplink, data_rate)
                print(f'processed {uplink=}, {data_rate=}')
        print(f'processed {uplink=}')

    if config.loss_rate_congestion:
        for uplink in [True]:
            print(f'processing {uplink=}')
            for data_rate in range(1, 6, 2):
                print(f'processing {uplink=}, {data_rate=}')

                res = {}
                for mdl in config.models:
                    new_params = dict(tcpVariant=mdl.value, nodes=10, uplink=uplink, dataRate=data_rate)
                    print(f'{mdl} is processing with {data_rate=} and {uplink=}')
                    throughput = collect_packet_loss_stats_error(new_params)
                    print(f'{mdl} is processed with {data_rate=} and {uplink=}')
                    res[mdl] = throughput

                with open(f'error_{data_rate}.res', 'w') as f:
                    f.write(f'{res}')
                
                create_throughput_plot(config, res, uplink, data_rate)
                print(f'processed {uplink=}, {data_rate=}')
        print(f'processed {uplink=}')


    if config.window_size:
        for uplink in [True]:
            print(f'processing {uplink=}')
            for data_rate in range(1, 6, 2):
                print(f'processing {uplink=}, {data_rate=}')

                res = {}
                for mdl in config.models:
                    new_params = dict(tcpVariant=mdl.value, nodes=10, uplink=uplink, dataRate=data_rate)
                    print(f'{mdl} is processing with {data_rate=} and {uplink=}')
                    throughput = collect_window_size(new_params)
                    print(f'{mdl} is processed with {data_rate=} and {uplink=}')
                    res[mdl] = throughput

                with open(f'error_{data_rate}.res', 'w') as f:
                    f.write(f'{res}')
                
                create_throughput_plot(config, res, uplink, data_rate)
                print(f'processed {uplink=}, {data_rate=}')
        print(f'processed {uplink=}')

    if config.window_size_adapt:
        for uplink in [True]:
            print(f'processing {uplink=}')
            for data_rate in range(5, 6, 2):
                print(f'processing {uplink=}, {data_rate=}')

                res = {}
                for mdl in config.models:
                    new_params = dict(tcpVariant=mdl.value, dataRate=data_rate)
                    print(f'{mdl} is processing with {data_rate=} and {uplink=}')
                    throughput = collect_window_size_adapt(new_params)
                    print(f'{mdl} is processed with {data_rate=} and {uplink=}')
                    res[mdl] = throughput

                with open(f'error_{data_rate}.res', 'w') as f:
                    f.write(f'{res}')
                
                create_throughput_plot(config, res, uplink, data_rate)
                print(f'processed {uplink=}, {data_rate=}')
        print(f'processed {uplink=}')

def run_once(
    params: dict
):
    os.system(f'./ns3 run "scratch/cerl-exp/topology {_params_to_command_args(params)}"')


def collect_packet_loss_stats_dist(params) -> list[tuple[int, float]]:
    def run_once(params):
        os.system(f'./ns3 run "scratch/cerl-exp/topology {_params_to_command_args(params)}"')
        line = next(filter(lambda s: s.startswith('average from all:'), open('./topology-aggregated.throughput').readlines()))
        return float(line.removeprefix('average from all: '))
    
    throughputs = []
    for i in range(95, 106):
        print(f'calculating for distance {i}m')
        params.update(dict(distanceToAP=i, simulationTime=20))
        throughputs.append((i, run_once(params)))
    
    return throughputs


def collect_packet_loss_stats_error(params) -> list[tuple[int, float]]:
    def run_once(params):
        os.system(f'./ns3 run "scratch/cerl-exp/topology {_params_to_command_args(params)}"')
        line = next(filter(lambda s: s.startswith('average from all:'), open('./topology-aggregated.throughput').readlines()))
        return float(line.removeprefix('average from all: '))
    
    throughputs = []
    for i in range(0, 11):
        print(f'calculating for error rate {i}%')
        params.update(dict(distanceToAP=1, simulationTime=20, errorRate=i / 100))
        throughputs.append((i, run_once(params)))
    
    return throughputs


def collect_packet_loss_stats_cong(params) -> list[tuple[int, float]]:
    def run_once(params):
        os.system(f'./ns3 run "scratch/cerl-exp/topology {_params_to_command_args(params)}"')
        line = next(filter(lambda s: s.startswith('average from all:'), open('./topology-aggregated.throughput').readlines()))
        return float(line.removeprefix('average from all: '))
    
    throughputs = []
    for i in range(0, 11):
        print(f'calculating for loss in {i} mbps')
        params.update(dict(distanceToAP=1, simulationTime=30, lLost=i))
        throughputs.append((i, run_once(params)))
    
    return throughputs


def collect_window_size(params) -> list[tuple[int, float]]:
    def run_once(params):
        os.system(f'./ns3 run "scratch/cerl-exp/topology {_params_to_command_args(params)}"')
        line = next(filter(lambda s: s.startswith('average from all:'), open('./topology-aggregated.throughput').readlines()))
        return float(line.removeprefix('average from all: '))
    
    throughputs = []
    for i in range(1, 15):
        print(f'calculating for window size {i * 5}')
        params.update(dict(distanceToAP=1, simulationTime=30, errorRate=.1, cerlWindow=i * 5))
        throughputs.append((i * 5, run_once(params)))
    
    return throughputs


def collect_window_size_adapt(params) -> list[tuple[int, float]]:
    def run_once(params):
        os.system(f'./ns3 run "scratch/delay-change {_params_to_command_args(params)}"')
        line = next(filter(lambda s: s.startswith('max delay:'), open('./topology.throughput').readlines()))
        return int(line.removeprefix('max delay: '))
    
    throughputs = []
    for i in range(1, 15):
        print(f'calculating for window size {i * 5}')
        params.update(dict(delayAtOtherRoute=90, cerlWindow=i * 5))
        throughputs.append((i * 5, run_once(params)))
    
    return throughputs


def collect_throughput_dack(recalc=False, case='awnd', rng_runs=1) -> dict[tuple, list[tuple[float, float]]]:
    match case:
        case 'awnd':
            case_specific = dict(dtime=True, dtimeLimit=True, cwndEnabled=False)
        case 'cwnd':
            case_specific = dict(dtime=True, dtimeLimit=True, cwndEnabled=True)
        case 'no-limit':
            case_specific = dict(dtime=True, dtimeLimit=False)
        case 'default':
            case_specific = dict(dtime=False)
        case _:
            raise ValueError(f'unknown {case=}')

    def get_name():
        match case:
            case 'awnd':
                return 'limited-dtime'
            case 'cwnd':
                return 'limited-dtime-cwnd'
            case 'no-limit':
                return 'dtime'
            case 'default':
                return 'default'
            case _:
                raise ValueError(f'unknown {case=}')

    res = {}

    scenarios = [(1, 0)]#((1, 0), (2, 0), (3, 0), (4, 0), (1, 20), (4, 20))

    for dr in [15]:
        for tcpNodes, udpNodes in scenarios:
            params = dict(
                simulationTime=20,
                tcpNodes=tcpNodes,
                dataRate=dr // tcpNodes,
                udpNodes=udpNodes,
                **case_specific
            )

            def run_once(params):
                res = 0
                for seed in range(1, rng_runs + 1):
                    lambda_name = int(params['lambda'] * 100)
                        
                    algo_name = get_name()
                    params['rngSeed'] = seed

                    path = f'./results/topology-aggregated.throughput.{algo_name}{lambda_name}.dr-{dr // tcpNodes}.rng-{seed}.tcp-{tcpNodes}.udp-{udpNodes}.delayed'

                    if not (os.path.exists(path) and len(open(path).readlines()) > 1) or recalc:
                        os.system(f'./ns3 run "scratch/real-example/topology {_params_to_command_args(params)}"')
                    
                    line = next(filter(lambda s: s.startswith('average from all:'), open(path).readlines()))
                    res += float(line.removeprefix('average from all: '))
                return res / rng_runs

            throughputs = []
            l, r = 100, 500
            step = 50

            for i in range(l, r + 1, step):
                print(f'calculating for lambda {i / 100}, {dr=}, {tcpNodes=}, {udpNodes=}')
                params.update({'lambda': i / 100})
                throughputs.append((i / 100, run_once(params)))

                if case == 'default':
                    throughputs = [(lam / 100, throughputs[-1][1]) for lam in range(l, r + 1, step)]
                    break

            res[(dr, tcpNodes, udpNodes)] = throughputs
            
    return res


def _params_to_command_args(params: dict):
    return ' '.join([f'--{k}={v}' for k, v in params.items()])
