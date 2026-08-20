"""Mini LLM inference experiments for InferLab."""

from .kv_cache import KVCacheConfig, KVCacheEstimate, estimate_kv_cache
from .transformer import (
    MiniKVCacheEntry,
    MiniTransformerConfig,
    MiniTransformerOutput,
    build_model,
    generate_greedy,
    run_generation_trace,
)
from .cache_compare import compare_cache_strategies
from .bottleneck_advisor import build_bottleneck_report
from .scenario_sweep import Scenario, build_scenario_sweep, default_scenarios
from .speculative_decode import (
    SpeculativeScenario,
    build_speculative_decode_report,
    default_scenarios as default_speculative_scenarios,
)
from .workload_profile import build_strategy_profile, build_workload_report

__all__ = [
    "build_bottleneck_report",
    "build_scenario_sweep",
    "build_speculative_decode_report",
    "build_strategy_profile",
    "build_workload_report",
    "compare_cache_strategies",
    "default_scenarios",
    "default_speculative_scenarios",
    "KVCacheConfig",
    "KVCacheEstimate",
    "MiniKVCacheEntry",
    "MiniTransformerConfig",
    "MiniTransformerOutput",
    "build_model",
    "estimate_kv_cache",
    "generate_greedy",
    "run_generation_trace",
    "Scenario",
    "SpeculativeScenario",
]
