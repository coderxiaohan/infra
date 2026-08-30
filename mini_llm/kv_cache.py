"""KV-cache shape and memory estimation utilities.

This module is intentionally dependency-light so the repository can verify the
first inference concepts before PyTorch/CUDA are installed.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Dict, Tuple

from .cli_io import require_positive_int


DTYPE_BITS: Dict[str, int] = {
    "fp32": 32,
    "float32": 32,
    "bf16": 16,
    "fp16": 16,
    "float16": 16,
    "int8": 8,
    "int4": 4,
}

@dataclass(frozen=True)
class KVCacheConfig:
    batch_size: int
    sequence_length: int
    num_layers: int
    num_heads: int
    head_dim: int
    dtype: str = "fp16"

    def validate(self) -> None:
        fields = {
            "batch_size": self.batch_size,
            "sequence_length": self.sequence_length,
            "num_layers": self.num_layers,
            "num_heads": self.num_heads,
            "head_dim": self.head_dim,
        }
        for name, value in fields.items():
            require_positive_int(name, value)
        if self.dtype not in DTYPE_BITS:
            known = ", ".join(sorted(DTYPE_BITS))
            raise ValueError(f"unknown dtype {self.dtype!r}; known: {known}")


@dataclass(frozen=True)
class KVCacheEstimate:
    key_shape: Tuple[int, int, int, int]
    value_shape: Tuple[int, int, int, int]
    bytes_per_token_per_layer: int
    total_bytes: int

    @property
    def total_mib(self) -> float:
        return self.total_bytes / (1024 * 1024)

    def to_dict(self) -> Dict[str, object]:
        data = asdict(self)
        data["total_mib"] = self.total_mib
        return data


def estimate_kv_cache(config: KVCacheConfig) -> KVCacheEstimate:
    """Estimate full KV-cache shape and memory.

    Shape convention: [batch, heads, sequence, head_dim].
    Memory formula:
      ceil(2(K,V) * batch * heads * head_dim * dtype_bits / 8)
      * sequence * layers
    """

    config.validate()
    dtype_bits = DTYPE_BITS[config.dtype]
    key_shape = (
        config.batch_size,
        config.num_heads,
        config.sequence_length,
        config.head_dim,
    )
    value_shape = key_shape
    values_per_token_per_layer = 2 * config.batch_size * config.num_heads * config.head_dim
    bytes_per_token_per_layer = (values_per_token_per_layer * dtype_bits + 7) // 8
    total_bytes = bytes_per_token_per_layer * config.sequence_length * config.num_layers
    return KVCacheEstimate(
        key_shape=key_shape,
        value_shape=value_shape,
        bytes_per_token_per_layer=bytes_per_token_per_layer,
        total_bytes=total_bytes,
    )
