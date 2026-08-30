"""Script helpers exposed for direct-file and ``python -m scripts.*`` entrypoints.

Most InferLab Python files under this package are glue scripts that support
both direct execution and ``python3 -m scripts.<module>``. Adding the package
directory to ``sys.path`` preserves their sibling imports in both forms.
"""

from __future__ import annotations

import sys
from pathlib import Path

_SCRIPTS_DIR = Path(__file__).resolve().parent
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))
