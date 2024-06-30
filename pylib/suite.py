from copy import deepcopy
from typing import Iterable
from functools import reduce
import asyncio
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
import os
import pandas as pd
import numpy as np

from .configuration import Suite, SuiteConfig, Case

sem = asyncio.Semaphore(30)

async def get_result(path: str, suite: Suite, recalc: bool):
    # if not (os.path.exists(path) and len(open(path).readlines()) > 1) or recalc:
    #     async with sem:
    #         print(suite)
    #         proc = await asyncio.create_subprocess_shell(f'./ns3 run "scratch/real-example/topology {_params_to_command_args(suite.dump_params())}"')
    #         await proc.wait()
    
    if not (os.path.exists(path) and len(open(path).readlines()) > 1):
        return None
    
    line = next(filter(lambda s: s.startswith('average from all:'), open(path).readlines()))
    
    return float(line.removeprefix('average from all: '))


async def run_once(suite: Suite, recalc: bool):
    results = []

    for seed in range(1, suite.rng_runs + 1):
        algo_name = suite.case.algo_name
        path = f'./results/topology-aggregated.throughput.{algo_name}{suite.beta}.dr-{suite.dataRate}.rng-{seed}.tcp-{suite.tcpNodes}.udp-{suite.udpNodes}.fortyHz-{int(suite.fortyHz)}.mobile-{int(suite.mobility)}.distance-{suite.distance}.tx-{suite.tx}{"" if suite.uplink else ".downlink"}{"" if suite.tcpVariant == "TcpLinuxReno" else f".ns3::{suite.tcpVariant}"}.delayed'
        results.append(asyncio.create_task(get_result(path, suite, recalc)))
    
    res = await asyncio.gather(*results)
    runs = suite.rng_runs - sum([1 for result in res if result is None])

    return {suite: suite.tcpNodes * sum(filter(None, res)) / runs if runs else 0}


async def collect_throughput_dack(suite_configs: list[SuiteConfig], case: Case, recalc=False) -> dict[str, dict[Suite, float | int]]:
    global sem
    sem = asyncio.Semaphore(20)

    results = {}

    for config in suite_configs:
        suite_results = reduce(
            lambda dict, new_dict: {**dict, **new_dict}, 
            await asyncio.gather(*[run_once(suite, recalc) for suite in config.get_all_experiments(case)]), 
            {}
        )
        if config.name not in results:
            results[config.name] = {}
        
        results[config.name] = {**results[config.name], **suite_results}

    return results


def _params_to_command_args(params: dict):
    return ' '.join([f'--{k}={v}' for k, v in params.items()])
