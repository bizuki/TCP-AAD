from pylib.configuration import *
from pylib.suite import process_suite
from pylib.plots import create_delay_plot, create_cmp_plot

SUITES = [
    # SuiteConfig(name='loss_rate_error', loss_rate_error=True, models=Model.interesting()),
    # SuiteConfig(name='loss_rate_dist', loss_rate_dist=True, models=Model.interesting()),
    # SuiteConfig(name='loss_rate_cong', loss_rate_congestion=True, models=Model.interesting()),
    # SuiteConfig(name='window_size', window_size=True, models=Model.interesting()),
    SuiteConfig(name='window_size_adapt', window_size_adapt=True, models=Model.interesting()),
]
    
for suite in SUITES:
    # process_suite(suite)
    create_delay_plot('topology.delay')
    # create_cmp_plot('topology.cwnd')
