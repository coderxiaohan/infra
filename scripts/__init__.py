"""Script helpers exposed for direct-file and ``python -m scripts.*`` entrypoints.

Most InferLab Python files under this package are glue scripts that historically
supported direct execution, for example ``python3 scripts/render_final_report.py``.
Adding the package directory to ``sys.path`` preserves those same sibling
imports when a command is normalized to ``python3 -m scripts.render_final_report``.
"""

from __future__ import annotations

import sys
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))
