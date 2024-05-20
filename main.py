from pylib.configuration import *
from pylib.suite import *
from pylib.plots import *

SUITES = [
    # SuiteConfig(name='loss_rate_error', loss_rate_error=True, models=Model.interesting()),
    # SuiteConfig(name='loss_rate_dist', loss_rate_dist=True, models=Model.interesting()),
    # SuiteConfig(name='loss_rate_cong', loss_rate_congestion=True, models=Model.interesting()),
    # SuiteConfig(name='window_size', window_size=True, models=Model.interesting()),
    SuiteConfig(name='window_size_adapt', window_size_adapt=True, models=Model.interesting()),
]
    
for suite in SUITES:
    # process_suite(suite)
    # create_delay_plot('topology.delay')
    # collect_throughput_dack(True, 'delayed')
    {name: collect_throughput_dack(False, name, rng_runs=2) for name in ('default', 'no-limit', 'cwnd', 'awnd')}
    # create_cmp_plot('topology.cwnd')
