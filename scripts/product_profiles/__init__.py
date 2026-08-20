"""InferLab product profile adapters.

These scripts integrate mature inference products as external backends. They
do not reimplement serving engines, KV connectors, kernel libraries, or cloud
control planes.
"""

from __future__ import annotations

from typing import List


def product_profile_module(product_key: str) -> str:
    return f"scripts.product_profiles.profile_{product_key}"


def product_profile_command_prefix(product_key: str) -> List[str]:
    return ["python3", "-m", product_profile_module(product_key)]
