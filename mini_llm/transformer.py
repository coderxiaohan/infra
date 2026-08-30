"""PyTorch Mini Transformer reference for InferLab.

This module is intentionally small, but it keeps the core decoder-only
inference mechanics explicit: RMSNorm, RoPE, causal attention, SwiGLU, KV cache,
prefill, and decode. It is a correctness/reference path for learning and trace
generation, not a performance-optimized runtime.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any, Iterable, Sequence

from .cli_io import (
    require_int_range,
    require_non_negative_int,
    require_positive_float,
    require_positive_int,
)

try:
    import torch
    from torch import Tensor, nn
    import torch.nn.functional as F
except ModuleNotFoundError:  # pragma: no cover - covered by dependency-light imports.
    torch = None  # type: ignore[assignment]
    Tensor = Any  # type: ignore[misc, assignment]
    nn = None  # type: ignore[assignment]
    F = None  # type: ignore[assignment]


TorchModule = nn.Module if nn is not None else object


def require_torch():
    if torch is None:
        raise RuntimeError(
            "PyTorch is required for mini_llm.transformer. "
            "Install torch in the project environment before running M1 model code."
        )
    return torch


@dataclass(frozen=True)
class MiniTransformerConfig:
    vocab_size: int = 256
    max_seq_len: int = 256
    hidden_size: int = 64
    num_layers: int = 2
    num_heads: int = 4
    ffn_hidden_size: int = 256
    rope_theta: float = 10000.0
    dtype: str = "float32"
    device: str = "cpu"
    use_bias: bool = False

    @property
    def head_dim(self) -> int:
        return self.hidden_size // self.num_heads

    def validate(self) -> None:
        require_positive_int("vocab_size", self.vocab_size)
        require_positive_int("max_seq_len", self.max_seq_len)
        require_positive_int("hidden_size", self.hidden_size)
        require_positive_int("num_layers", self.num_layers)
        require_positive_int("num_heads", self.num_heads)
        require_positive_int("ffn_hidden_size", self.ffn_hidden_size)
        require_positive_float("rope_theta", self.rope_theta)
        if self.hidden_size % self.num_heads != 0:
            raise ValueError("hidden_size must be divisible by num_heads")
        if self.head_dim % 2 != 0:
            raise ValueError("RoPE requires an even attention head dimension")

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self)
        data["head_dim"] = self.head_dim
        return data


@dataclass
class MiniKVCacheEntry:
    key: Tensor
    value: Tensor

    @property
    def sequence_length(self) -> int:
        return int(self.key.shape[2])

    @property
    def byte_size(self) -> int:
        return int(
            self.key.numel() * self.key.element_size()
            + self.value.numel() * self.value.element_size()
        )


@dataclass
class MiniTransformerOutput:
    logits: Tensor
    kv_cache: list[MiniKVCacheEntry]
    hidden_states: Tensor

    def to_metadata(self) -> dict[str, Any]:
        kv_lengths = [entry.sequence_length for entry in self.kv_cache]
        kv_bytes = [entry.byte_size for entry in self.kv_cache]
        return {
            "logits_shape": list(self.logits.shape),
            "hidden_shape": list(self.hidden_states.shape),
            "kv_cache_layers": len(self.kv_cache),
            "kv_cache_lengths": kv_lengths,
            "kv_cache_bytes": kv_bytes,
            "kv_cache_total_bytes": int(sum(kv_bytes)),
        }


def torch_dtype(name: str):
    torch_mod = require_torch()
    mapping = {
        "float32": torch_mod.float32,
        "fp32": torch_mod.float32,
        "float16": torch_mod.float16,
        "fp16": torch_mod.float16,
        "bfloat16": torch_mod.bfloat16,
        "bf16": torch_mod.bfloat16,
    }
    try:
        return mapping[name.lower()]
    except KeyError as exc:
        raise ValueError(f"Unsupported dtype: {name}") from exc


class RMSNorm(TorchModule):
    def __init__(self, hidden_size: int, eps: float = 1e-6) -> None:
        require_torch()
        super().__init__()
        self.eps = eps
        self.weight = nn.Parameter(torch.ones(hidden_size))

    def forward(self, hidden_states: Tensor) -> Tensor:
        input_dtype = hidden_states.dtype
        float_states = hidden_states.float()
        variance = float_states.pow(2).mean(dim=-1, keepdim=True)
        normed = float_states * torch.rsqrt(variance + self.eps)
        return (normed * self.weight.float()).to(dtype=input_dtype)


def apply_rope(x: Tensor, positions: Tensor, theta: float = 10000.0) -> Tensor:
    """Apply rotary embeddings to [batch, heads, seq, head_dim] tensors."""

    require_torch()
    head_dim = x.shape[-1]
    if head_dim % 2 != 0:
        raise ValueError("RoPE requires an even last dimension")

    freq_ids = torch.arange(0, head_dim, 2, device=x.device, dtype=torch.float32)
    inv_freq = 1.0 / (theta ** (freq_ids / head_dim))
    angles = positions.to(device=x.device, dtype=torch.float32).unsqueeze(1) * inv_freq
    cos = angles.cos().to(dtype=x.dtype).view(1, 1, positions.numel(), head_dim // 2)
    sin = angles.sin().to(dtype=x.dtype).view(1, 1, positions.numel(), head_dim // 2)

    x_even = x[..., 0::2]
    x_odd = x[..., 1::2]
    rotated = torch.stack((x_even * cos - x_odd * sin, x_even * sin + x_odd * cos), dim=-1)
    return rotated.flatten(-2)


class CausalSelfAttention(TorchModule):
    def __init__(self, config: MiniTransformerConfig) -> None:
        require_torch()
        super().__init__()
        self.config = config
        self.qkv_proj = nn.Linear(
            config.hidden_size,
            3 * config.hidden_size,
            bias=config.use_bias,
        )
        self.out_proj = nn.Linear(
            config.hidden_size,
            config.hidden_size,
            bias=config.use_bias,
        )

    def forward(
        self,
        hidden_states: Tensor,
        positions: Tensor,
        past_kv: MiniKVCacheEntry | None = None,
        use_cache: bool = True,
    ) -> tuple[Tensor, MiniKVCacheEntry | None]:
        batch_size, seq_len, _ = hidden_states.shape
        config = self.config
        qkv = self.qkv_proj(hidden_states)
        qkv = qkv.view(batch_size, seq_len, 3, config.num_heads, config.head_dim)
        qkv = qkv.permute(2, 0, 3, 1, 4)
        query, key, value = qkv[0], qkv[1], qkv[2]

        query = apply_rope(query, positions, config.rope_theta)
        key = apply_rope(key, positions, config.rope_theta)

        if past_kv is not None:
            key_all = torch.cat((past_kv.key, key), dim=2)
            value_all = torch.cat((past_kv.value, value), dim=2)
        else:
            key_all = key
            value_all = value

        key_positions = torch.arange(key_all.shape[2], device=hidden_states.device)
        allowed = key_positions.unsqueeze(0) <= positions.to(hidden_states.device).unsqueeze(1)

        scale = config.head_dim ** -0.5
        scores = torch.matmul(query, key_all.transpose(-2, -1)) * scale
        scores = scores.masked_fill(~allowed.view(1, 1, seq_len, key_all.shape[2]), float("-inf"))
        probs = F.softmax(scores, dim=-1)
        context = torch.matmul(probs, value_all)
        context = context.transpose(1, 2).contiguous().view(batch_size, seq_len, config.hidden_size)
        output = self.out_proj(context)

        next_cache = MiniKVCacheEntry(key_all, value_all) if use_cache else None
        return output, next_cache


class SwiGLUFeedForward(TorchModule):
    def __init__(self, config: MiniTransformerConfig) -> None:
        require_torch()
        super().__init__()
        self.gate_up_proj = nn.Linear(
            config.hidden_size,
            2 * config.ffn_hidden_size,
            bias=config.use_bias,
        )
        self.down_proj = nn.Linear(
            config.ffn_hidden_size,
            config.hidden_size,
            bias=config.use_bias,
        )

    def forward(self, hidden_states: Tensor) -> Tensor:
        gate, up = self.gate_up_proj(hidden_states).chunk(2, dim=-1)
        return self.down_proj(F.silu(gate) * up)


class MiniDecoderLayer(TorchModule):
    def __init__(self, config: MiniTransformerConfig) -> None:
        require_torch()
        super().__init__()
        self.attention_norm = RMSNorm(config.hidden_size)
        self.attention = CausalSelfAttention(config)
        self.ffn_norm = RMSNorm(config.hidden_size)
        self.feed_forward = SwiGLUFeedForward(config)

    def forward(
        self,
        hidden_states: Tensor,
        positions: Tensor,
        past_kv: MiniKVCacheEntry | None = None,
        use_cache: bool = True,
    ) -> tuple[Tensor, MiniKVCacheEntry | None]:
        residual = hidden_states
        attn_output, next_cache = self.attention(
            self.attention_norm(hidden_states),
            positions=positions,
            past_kv=past_kv,
            use_cache=use_cache,
        )
        hidden_states = residual + attn_output
        hidden_states = hidden_states + self.feed_forward(self.ffn_norm(hidden_states))
        return hidden_states, next_cache


class MiniDecoderOnlyTransformer(TorchModule):
    def __init__(self, config: MiniTransformerConfig) -> None:
        require_torch()
        config.validate()
        super().__init__()
        self.config = config
        self.token_embedding = nn.Embedding(config.vocab_size, config.hidden_size)
        self.layers = nn.ModuleList([MiniDecoderLayer(config) for _ in range(config.num_layers)])
        self.final_norm = RMSNorm(config.hidden_size)
        self.lm_head = nn.Linear(config.hidden_size, config.vocab_size, bias=False)
        self.to(device=torch.device(config.device), dtype=torch_dtype(config.dtype))

    def forward(
        self,
        input_ids: Tensor,
        past_kv: Sequence[MiniKVCacheEntry | None] | None = None,
        use_cache: bool = True,
    ) -> MiniTransformerOutput:
        require_torch()
        if input_ids.dim() != 2:
            raise ValueError("input_ids must have shape [batch, sequence]")
        if input_ids.numel() == 0:
            raise ValueError("input_ids must not be empty")

        device = next(self.parameters()).device
        input_ids = input_ids.to(device=device, dtype=torch.long)
        past = list(past_kv) if past_kv is not None else [None] * len(self.layers)
        if len(past) != len(self.layers):
            raise ValueError("past_kv length must match num_layers")

        first_cache = next((entry for entry in past if entry is not None), None)
        past_len = first_cache.sequence_length if first_cache is not None else 0
        seq_len = int(input_ids.shape[1])
        if past_len + seq_len > self.config.max_seq_len:
            raise ValueError("sequence length exceeds max_seq_len")

        positions = torch.arange(past_len, past_len + seq_len, device=device)
        hidden_states = self.token_embedding(input_ids)
        next_cache: list[MiniKVCacheEntry] = []
        for layer, layer_past in zip(self.layers, past):
            hidden_states, layer_cache = layer(
                hidden_states,
                positions=positions,
                past_kv=layer_past,
                use_cache=use_cache,
            )
            if use_cache and layer_cache is not None:
                next_cache.append(layer_cache)

        hidden_states = self.final_norm(hidden_states)
        logits = self.lm_head(hidden_states)
        return MiniTransformerOutput(
            logits=logits,
            kv_cache=next_cache,
            hidden_states=hidden_states,
        )


def build_model(
    config: MiniTransformerConfig | None = None,
    *,
    seed: int = 0,
) -> MiniDecoderOnlyTransformer:
    torch_mod = require_torch()
    config = config or MiniTransformerConfig()
    config.validate()
    torch_mod.manual_seed(seed)
    model = MiniDecoderOnlyTransformer(config)
    model.eval()
    return model


def _tensor_from_prompt(prompt_ids: Sequence[int], *, device: str | torch.device) -> Tensor:
    require_torch()
    if not prompt_ids:
        raise ValueError("prompt_ids must not be empty")
    return torch.tensor([list(prompt_ids)], dtype=torch.long, device=device)


def generate_greedy(
    model: MiniDecoderOnlyTransformer,
    prompt_ids: Sequence[int],
    *,
    max_new_tokens: int,
    use_cache: bool = True,
) -> tuple[list[int], list[MiniKVCacheEntry]]:
    require_torch()
    require_non_negative_int("max_new_tokens", max_new_tokens)

    device = next(model.parameters()).device
    generated = _normalize_prompt_ids(prompt_ids, model.config.vocab_size)
    input_ids = _tensor_from_prompt(generated, device=device)
    cache: list[MiniKVCacheEntry] = []
    with torch.no_grad():
        output = model(input_ids, use_cache=use_cache)
        cache = output.kv_cache
        next_logits = output.logits[:, -1, :]
        for _ in range(max_new_tokens):
            next_token = int(torch.argmax(next_logits, dim=-1).item())
            generated.append(next_token)
            if use_cache:
                step_input = torch.tensor([[next_token]], dtype=torch.long, device=device)
                output = model(step_input, past_kv=cache, use_cache=True)
                cache = output.kv_cache
            else:
                step_input = _tensor_from_prompt(generated, device=device)
                output = model(step_input, use_cache=False)
                cache = []
            next_logits = output.logits[:, -1, :]
    return generated, cache


def _normalize_prompt_ids(prompt_ids: Iterable[int], vocab_size: int) -> list[int]:
    normalized = [
        require_int_range(f"prompt_ids[{index}]", token, 0, vocab_size - 1)
        for index, token in enumerate(prompt_ids)
    ]
    if not normalized:
        raise ValueError("prompt_ids must not be empty")
    return normalized


def run_generation_trace(
    config: MiniTransformerConfig | None,
    prompt_ids: Sequence[int],
    *,
    max_new_tokens: int,
    seed: int = 0,
    use_cache: bool = True,
) -> dict[str, Any]:
    require_torch()
    config = config or MiniTransformerConfig()
    config.validate()
    require_non_negative_int("max_new_tokens", max_new_tokens)

    prompt = _normalize_prompt_ids(prompt_ids, config.vocab_size)
    model = build_model(config, seed=seed)
    device = next(model.parameters()).device
    generated = list(prompt)
    records: list[dict[str, Any]] = []

    with torch.no_grad():
        prefill_input = _tensor_from_prompt(generated, device=device)
        output = model(prefill_input, use_cache=use_cache)
        cache = output.kv_cache
        next_logits = output.logits[:, -1, :]
        records.append(
            {
                "phase": "prefill",
                "step": 0,
                "input_ids": list(prompt),
                "generated_ids": list(generated),
                "argmax_next_token": int(torch.argmax(next_logits, dim=-1).item()),
                **output.to_metadata(),
            }
        )

        for step in range(1, max_new_tokens + 1):
            next_token = int(torch.argmax(next_logits, dim=-1).item())
            generated.append(next_token)
            if use_cache:
                decode_input_ids = [next_token]
                decode_input = torch.tensor([[next_token]], dtype=torch.long, device=device)
                output = model(decode_input, past_kv=cache, use_cache=True)
                cache = output.kv_cache
            else:
                decode_input_ids = list(generated)
                decode_input = _tensor_from_prompt(generated, device=device)
                output = model(decode_input, use_cache=False)
                cache = []
            next_logits = output.logits[:, -1, :]
            records.append(
                {
                    "phase": "decode",
                    "step": step,
                    "input_ids": decode_input_ids,
                    "generated_ids": list(generated),
                    "argmax_next_token": int(torch.argmax(next_logits, dim=-1).item()),
                    **output.to_metadata(),
                }
            )

    return {
        "schema_version": 1,
        "kind": "mini_transformer_generation_trace",
        "torch_version": torch.__version__,
        "seed": seed,
        "use_cache": use_cache,
        "config": config.to_dict(),
        "prompt_ids": list(prompt),
        "max_new_tokens": max_new_tokens,
        "generated_ids": generated,
        "records": records,
        "notes": [
            "CPU/GPU device is selected by config.device.",
            "KV cache bytes are tensor storage bytes for key/value tensors only.",
            "This reference is for correctness and traceability, not optimized serving.",
        ],
    }
