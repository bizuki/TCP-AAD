from pylib.configuration import *
from pylib.suite import *
from pylib.plots import *

SUITES = [
    SuiteConfig(
        name='common_only_tcp',
        tcpVariant='TcpLinuxReno',
        simulationTime=20,
        tcpNodes=[1, 2, 3, 4],
        targetDataRate=[15, 70],
        udpNodes=[0],
        mobility=[False, True],
        tx=16,
        distance=70,
        uplink=[False, True],
        fortyHz=False,
        rng_runs=1,
        beta=list(range(100, 400, 100)),
    ),
    SuiteConfig(
        name='common_with_udp',
        tcpVariant='TcpLinuxReno',
        simulationTime=20,
        tcpNodes=[1, 2, 3, 4],
        udpNodes=[15],
        targetDataRate=[15, 70],
        mobility=[False, True],
        tx=16,
        distance=70,
        uplink=[False, True],
        fortyHz=False,
        rng_runs=1,
        beta=list(range(100, 400, 100)),
    ),
    SuiteConfig(
        name='varying_distance',
        tcpVariant='TcpLinuxReno',
        targetDataRate=70,
        tx=16,
        fortyHz=False,
        mobility=False,
        uplink=False,
        tcpNodes=1,
        udpNodes=0,
        simulationTime=20,
        distance=list(range(100, 160, 5)),
        beta=list(range(100, 400, 100)),
        rng_runs=1
    ),
    SuiteConfig(
        name='various_ccm',
        targetDataRate=70,
        tx=16,
        fortyHz=False,
        mobility=False,
        uplink=False,
        tcpNodes=1,
        udpNodes=0,
        simulationTime=20,
        distance=70,
        tcpVariant=['TcpLinuxReno', 'TcpCubic', 'TcpYeah', 'TcpWestwoodPlus'],
        beta=list(range(100, 400, 100)),
        rng_runs=1
    )
]

data = asyncio.run(collect_throughput_dack(SUITES, [Case.AAD, Case.DEFAULT], False))

def get_group_name(beta, case):
    match case:
        case Case.AAD:
            return f'TCP-AAD with $\\beta = {beta}$'
        case _:
            return f'TCP-{case.value}'
        

comparison_bars(
    data['common_only_tcp'],
    BarConfig(
        name='common_case_tcp', dir='new_plots',
        group_by_columns=['beta', 'case'], case_columns=['tcpNodes', 'udpNodes'],
        group_by_name=lambda t: get_group_name(*t), case_name=lambda t: f'TCP {t[0]}, UDP {t[1]}',
        title='Throughput for different algos', group_title='Algos'
    )
)

comparison_bars(
    data['common_with_udp'],
    BarConfig(
        name='common_case_udp', dir='new_plots',
        group_by_columns=['beta', 'case'], case_columns=['tcpNodes', 'udpNodes'],
        group_by_name=lambda t: get_group_name(*t), case_name=lambda t: f'TCP {t[0]}, UDP {t[1]}',
        title='Throughput for different algos', group_title='Algos'
    )
)

comparison_bars(
    data['various_ccm'],
    BarConfig(
        name='various_ccm', dir='new_plots',
        group_by_columns=['beta', 'case'], case_columns=['tcpVariant'],
        group_by_name=lambda t: get_group_name(*t), case_name=lambda t: t[0],
        title='Throughput for different algos', group_title='Algos'
    )
)

create_cmp_plot(asyncio.run(get_case_detailed_stats(
    Suite(
        tcpVariant='TcpLinuxReno',
        targetDataRate=70,
        tx=16,
        fortyHz=False,
        mobility=False,
        uplink=False,
        tcpNodes=1,
        udpNodes=0,
        simulationTime=20,
        distance=100,
        beta=300,
        rng_runs=1,
        case=Case.DEFAULT
    ), 2
)))
