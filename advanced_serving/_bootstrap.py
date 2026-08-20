"""Direct-file execution bootstrap for advanced_serving entrypoints."""

from __future__ import annotations

import sys
from pathlib import Path


def bootstrap_package() -> None:
    package_root = Path(__file__).resolve().parents[1]
    if str(package_root) not in sys.path:
        sys.path.insert(0, str(package_root))
