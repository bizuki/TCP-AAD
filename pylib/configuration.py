from pydantic import BaseModel, model_validator, computed_field, ConfigDict
from dataclasses import dataclass, asdict
from pandas import DataFrame
from itertools import product
from functools import reduce
from enum import Enum

class Case(Enum):
    AAD = 'AAD'
    ADW = 'ADW'
    DEFAULT = 'default'
    
    @property
    def specific_params(self):
        match self:
            case Case.AAD:
                return dict(tcpAad=True, cwndEnabled=False)
            case Case.ADW:
                return {'lambda': 3, **dict(tcpAdw=True, cwndEnabled=True)}
            case Case.DEFAULT:
                 return dict(tcpAad=False, tcpAdw=False)
            case _:
                raise ValueError(f'unknown {self=}')
            
    @property
    def algo_name(self):
        match self:
            case Case.AAD:
                return 'tcpAad'
            case Case.ADW:
                return 'tcpAdw'
            case Case.DEFAULT:
                return 'default'
            case _:
                raise ValueError(f'unknown {self=}')
            

    def __lt__(self, other):
        if isinstance(other, Case):
            return self.value < other.value
        raise TypeError('bruh')
        
            

def is_iterable(obj):
    return isinstance(obj, list) and not isinstance(obj, str)


class Suite(BaseModel):
    model_config = ConfigDict(frozen=True)

    tcpVariant: str
    simulationTime: int
    tcpNodes: int
    targetDataRate: int
    udpNodes: int
    mobility: bool
    tx: int
    distance: int
    uplink: bool
    fortyHz: bool

    beta: int = 300

    case: Case
    rng_runs: int

    @computed_field
    @property
    def udpDataRate(self) -> int:
        return 4 if self.fortyHz else 2
    
    @computed_field
    @property
    def dataRate(self) -> int:
        return self.targetDataRate // self.tcpNodes
    
    def dump_params(self):
        return {'beta': self.beta / 100, **self.model_dump(exclude={'case', 'rng_runs', 'targetDataRate', 'beta'}, exclude_none=True), **self.case.specific_params}

    def get_params(self):
        return {'beta': self.beta / 100, **self.model_dump(exclude={'rng_runs', 'dataRate', 'beta', 'udpDataRate'}, exclude_none=True)}


class SuiteConfig(BaseModel):
    name: str

    tcpVariant: list[str]
    simulationTime: list[int]
    tcpNodes: list[int]
    targetDataRate: list[int]
    udpNodes: list[int]
    mobility: list[bool]
    tx: list[int]
    distance: list[int]
    uplink: list[bool]
    fortyHz: list[bool]
    beta: list[int]

    rng_runs: int

    def varying_params(self):
        return [k for k, v in self.model_dump().items() if isinstance(v, list) and len(v) > 1]

    @model_validator(mode='before')
    @classmethod
    def validate_all_fields_at_the_same_time(cls, field_values):
        return {k: v if is_iterable(v) or k in ('rng_runs', 'name') else [v] for k, v in field_values.items()}
    
    def get_all_experiments(self, case: Case) -> list[Suite]:
        excluded_fields = ['rng_runs', 'name']
        # specific only to AAD
        if case is not Case.AAD:
            excluded_fields += ['beta']

        return [
            Suite(**reduce(lambda dict, new_dict: {**dict, **new_dict}, params, dict(case=case, rng_runs=self.rng_runs)))
            for params in product(*[
                list(map(lambda i: {field: i}, value)) for field, value in self.model_dump(exclude=excluded_fields).items()
            ])
        ]


class DetailedStats(BaseModel):
    ts: float
    delay: float
    iat: float  # ms
    base_iat: float  # ms
    theta: float  # TCP-ADW 
    current_timeout: float # ms
    aggregation_id: int
    aggregation_position: int
    delayed_ack: int  # number of delayed acks
    delay_window: float
    sender_cwnd: float

    @property
    def dataframe(self):
        return DataFrame(
            data=self.model_dump(),
            index=['ts']
        )
