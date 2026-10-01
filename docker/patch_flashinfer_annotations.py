"""Backport FlashInfer's deferred-annotation fix for Python 3.10.

Upstream: https://github.com/flashinfer-ai/flashinfer/blob/main/flashinfer/comm/fd_exchange.py
The version and source hash guard keep this patch limited to the verified wheel.
"""
import hashlib
from importlib.metadata import distribution

dist = distribution("flashinfer-python")
if dist.version == "0.6.16.post3":
    path = dist.locate_file("flashinfer/comm/fd_exchange.py")
    original = path.read_bytes()
    expected = "6f9549238cc450efeb30aa740c0bdc2e6dfd4cfa29cee43a9ab010c90a407cee"
    if hashlib.sha256(original).hexdigest() != expected:
        raise RuntimeError("FlashInfer source differs from the verified wheel")
    marker = b"\nimport array\n"
    if original.count(marker) != 1:
        raise RuntimeError("FlashInfer annotation patch location is ambiguous")
    patched = original.replace(marker, b"\nfrom __future__ import annotations\n\nimport array\n", 1)
    path.write_bytes(patched)
    print("Applied upstream FlashInfer annotation fix:", hashlib.sha256(patched).hexdigest())
else:
    print("No annotation backport applied for FlashInfer", dist.version)
