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
        targetDataRate=[70, 120],
        mobility=[False, True],
        tx=16,
        distance=70,
        uplink=[False, True],
        fortyHz=True,
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
        beta=list(range(100, 500, 50)),
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
        beta=list(range(100, 500, 50)),
        rng_runs=1
    )
]

data = asyncio.run(collect_throughput_dack(SUITES, Case.AAD, False))

comparison_bars(
    data['common_only_tcp'],
    BarConfig(
        name='common_case_tcp', dir='new_plots',
        group_by_columns=['beta'], case_columns=['tcpNodes', 'udpNodes'],
        group_by_name=lambda t: f'beta={t[0]}', case_name=lambda t: f'TCP {t[0]}, UDP {t[1]}',
        title='Throughput at different betas', group_title='Betas'
    )
)

comparison_bars(
    data['common_with_udp'],
    BarConfig(
        name='common_case_udp', dir='new_plots',
        group_by_columns=['beta'], case_columns=['tcpNodes', 'udpNodes'],
        group_by_name=lambda t: f'beta={t[0]}', case_name=lambda t: f'TCP {t[0]}, UDP {t[1]}',
        title='Throughput at different betas', group_title='Betas'
    )
)
    
# for suite in SUITES:
    # process_suite(suite)
    # create_delay_plot('topology.delay')
    # collect_throughput_dack(True, 'delayed')
    # create_throughput_plot({name: asyncio.run(collect_throughput_dack(False, name, rng_runs=10)) for name in ('no-limit', 'default', 'dack')})
    # create_cmp_plot('results2/topology.cwnd.default150.dr-70.rng-1.tcp-1.udp-0.fortyHz-0.mobile-0.distance-50.tx-16.delayed')
    
    # create_aggr_plot('results2/topology.cwnd.dtime100.dr-70.rng-1.tcp-1.udp-0.fortyHz-0.mobile-0.distance-50.tx-16.delayed', 1)
    # create_aggr_plot('results2/topology.cwnd.dtime150.dr-70.rng-1.tcp-1.udp-0.fortyHz-0.mobile-0.distance-50.tx-16.delayed', 1.5)
    # create_aggr_plot('results2/topology.cwnd.dtime200.dr-70.rng-1.tcp-1.udp-0.fortyHz-0.mobile-0.distance-50.tx-16.delayed', 2)
    # create_aggr_plot('results2/topology.cwnd.dtime300.dr-70.rng-1.tcp-1.udp-0.fortyHz-0.mobile-0.distance-50.tx-16.delayed', 3)
