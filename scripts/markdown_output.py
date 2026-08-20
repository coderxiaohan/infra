#!/usr/bin/env python3
"""Shared Markdown value and table-cell formatting for InferLab reports."""

from __future__ import annotations

import json
from typing import Any


def format_markdown_value(value: Any) -> str:
    if value is None:
        return "_missing_"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        return f"{value:.6g}"
    if isinstance(value, (list, dict)):
        return f"`{json.dumps(value, sort_keys=True)}`"
    return str(value)


def escape_markdown_cell(value: Any) -> str:
    return format_markdown_value(value).replace("|", "\\|").replace("\n", "<br>")
