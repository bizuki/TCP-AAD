from pylib.configuration import *
from pylib.suite import process_suite

SUITES = [
    SuiteConfig(name='loss_rate_error', loss_rate_error=True, models=Model.interesting()),
    SuiteConfig(name='loss_rate_dist', loss_rate_dist=True, models=Model.interesting()),
    SuiteConfig(name='loss_rate_cong', loss_rate_congestion=True, models=Model.interesting()),
]
    
for suite in SUITES:
    process_suite(suite)
