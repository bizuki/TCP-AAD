from pydantic import BaseModel
from enum import Enum

class Model(Enum):
    CUBIC = 'TcpCubic'
    VENO = 'TcpVeno'
    NEW_RENO = 'TcpNewReno'
    WESTWOOD_PLUS = 'TcpWestwoodPlus'
    YEAH = 'TcpYeah'
    BBR = 'TcpBbr'
    CERL = 'TcpCerl'
    CERL_PLUS = 'TcpCerlPlus'
    CERL_PLUS_X = 'TcpCerlX'


class SuiteConfig(BaseModel):
    name: str
    loss_rate_dist: bool = False
    loss_rate_error: bool = False
    loss_rate_congestion: bool = False

    models: list[Model]

    @property
    def models_string(self):
        return ','.join([mdl.value for mdl in self.models])
    
    def to_params(self):
        return dict(tcpVariants=self.models_string)
