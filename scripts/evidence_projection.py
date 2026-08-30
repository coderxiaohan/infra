#!/usr/bin/env python3
"""Shared evidence loading and projection primitives for report/export glue."""

from __future__ import annotations

import math
from dataclasses import dataclass, fields as dataclass_fields
from functools import partial
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence, Tuple, Union

from command_redaction import redact_path_error, redacted_path_label as path_label
from json_projection import get_path
from path_status import safe_exists as safe_path_probe
from repo_paths import default_repo_root, resolve_repo_path


PathItem = Union[str, int]


def missing_preserving_default(value: Any, *, preserve_missing: bool) -> Any:
    return None if preserve_missing else value

CATALOG_FLOW_FALLBACK_REMOVAL_QUEUE_COUNT_FIELDS = (
    "catalog_flow_fallback_removal_queue_item_count",
    "catalog_flow_fallback_removal_queue_blocked_count",
    "catalog_flow_fallback_removal_queue_parity_refresh_required_count",
    "catalog_flow_fallback_removal_queue_current_repo_action_count",
    "catalog_flow_fallback_removal_queue_target_machine_action_count",
)

DOMAIN_VALUE_RAW = "raw"
DOMAIN_VALUE_COUNT = "count"
DOMAIN_VALUE_TRUTHY = "truthy"
DOMAIN_VALUE_NUMERIC_BOOL = "numeric_bool"


def project_catalog_flow_fallback_removal_queue(
    evidence_query: Mapping[str, Any],
) -> Dict[str, Any]:
    """Project canonical fallback-removal queue counts from Evidence Query."""
    projection: Dict[str, Any] = {
        "canonical_source": "native_evidence_query_result.json",
        "canonical_gap_count": 0,
        "source_drift_count": 0,
        "source_drift_fields": [],
        "source_drift_sources": [],
        "source_consistent": True,
    }

    for field_name in CATALOG_FLOW_FALLBACK_REMOVAL_QUEUE_COUNT_FIELDS:
        raw_value = evidence_query.get(field_name)
        canonical_valid = (
            isinstance(raw_value, int)
            and not isinstance(raw_value, bool)
            and raw_value >= 0
        )
        canonical_value = raw_value if canonical_valid else 0
        projection[field_name] = canonical_value
        if not canonical_valid:
            projection["canonical_gap_count"] += 1

    projection["canonical_ready"] = projection["canonical_gap_count"] == 0
    return projection


def _mapping_value(value: Any) -> Mapping[str, Any]:
    return value if isinstance(value, Mapping) else {}


def _first_non_none(*values: Any) -> Any:
    for value in values:
        if value is not None:
            return value
    return None


def dict_value(value: Any) -> Dict[str, Any]:
    """Return a borrowed dict value or an empty projection container."""
    return value if isinstance(value, dict) else {}


def list_value(value: Any) -> List[Any]:
    """Return a borrowed list value or an empty projection container."""
    return value if isinstance(value, list) else []


def artifact_summary(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Project the canonical artifact summary object without interpreting it."""
    return dict_value(data.get("summary")) if isinstance(data, dict) else {}


# Public projection alias. Existing internal users retain the private spelling.
first_present = _first_non_none


@dataclass(frozen=True)
class ReleasePipelineProductReplayProjection:
    evidence_query_cross_artifact_product_replay_family_present: bool = False
    evidence_query_cross_artifact_product_replay_family_status: Any = "not_reported"
    evidence_query_cross_artifact_product_replay_family_blocker_count: int = 0
    evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count: int = 0
    evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count: int = 0
    evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count: int = 0
    evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count: int = 0
    evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count: int = 0
    evidence_query_cross_artifact_product_replay_family_source_artifact_count: int = 0
    evidence_query_cross_artifact_product_replay_family_first_source_artifact: Any = "unknown"
    evidence_query_cross_artifact_product_replay_family_next_action: Any = "unknown"
    evidence_query_product_replay_semantic_linkage_present: bool = False
    evidence_query_product_replay_semantic_linkage_status: Any = "unknown"
    evidence_query_product_replay_semantic_events_present: bool = False
    evidence_query_product_replay_alignment_present: bool = False
    evidence_query_product_replay_semantic_linkage_read_issue_count: int = 0
    evidence_query_product_replay_semantic_linkage_pending_event_count: int = 0
    evidence_query_product_replay_semantic_linkage_sample_event_count: int = 0
    evidence_query_product_replay_semantic_linkage_sample_replay_present: bool = False
    evidence_query_product_replay_semantic_linkage_replay_supported_event_count: int = 0
    evidence_query_product_replay_semantic_linkage_aligned_rule_count: int = 0
    evidence_query_product_replay_semantic_linkage_pending_rule_count: int = 0
    evidence_query_product_replay_semantic_linkage_correlation_count: int = 0
    evidence_query_product_replay_semantic_linkage_ready_correlation_count: int = 0
    evidence_query_product_replay_semantic_linkage_blocked_correlation_count: int = 0
    evidence_query_product_replay_semantic_linkage_blocker_count: int = 0
    evidence_query_product_replay_semantic_linkage_target_or_validation_action_count: int = 0
    evidence_query_product_replay_semantic_linkage_first_blocker: Any = ""
    evidence_query_product_replay_semantic_linkage_next_action: Any = ""
    product_replay_semantic_linkage_repair_path_count: int = 0
    product_replay_semantic_linkage_repair_path_source_location_gap_count: int = 0
    evidence_query_product_replay_semantic_linkage_repair_path_count: int = 0
    evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count: int = 0

    def to_cross_artifact_closure_dict(self) -> Dict[str, Any]:
        return {
            "product_replay_family_present": self.evidence_query_cross_artifact_product_replay_family_present,
            "product_replay_family_status": self.evidence_query_cross_artifact_product_replay_family_status,
            "product_replay_family_blocker_count": self.evidence_query_cross_artifact_product_replay_family_blocker_count,
            "product_replay_family_semantic_linkage_blocker_count": self.evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count,
            "product_replay_family_replay_or_execute_missing_count": self.evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count,
            "product_replay_family_target_or_validation_action_count": self.evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count,
            "product_replay_family_target_product_replay_action_count": self.evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count,
            "product_replay_family_target_gpu_execute_action_count": self.evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count,
            "product_replay_family_source_artifact_count": self.evidence_query_cross_artifact_product_replay_family_source_artifact_count,
            "product_replay_family_first_source_artifact": self.evidence_query_cross_artifact_product_replay_family_first_source_artifact,
            "product_replay_family_next_action": self.evidence_query_cross_artifact_product_replay_family_next_action,
        }

    def to_semantic_linkage_closure_dict(self) -> Dict[str, Any]:
        return {
            "linkage_present": self.evidence_query_product_replay_semantic_linkage_present,
            "status": self.evidence_query_product_replay_semantic_linkage_status,
            "semantic_events_present": self.evidence_query_product_replay_semantic_events_present,
            "alignment_present": self.evidence_query_product_replay_alignment_present,
            "read_issue_count": self.evidence_query_product_replay_semantic_linkage_read_issue_count,
            "pending_event_count": self.evidence_query_product_replay_semantic_linkage_pending_event_count,
            "sample_event_count": self.evidence_query_product_replay_semantic_linkage_sample_event_count,
            "sample_replay_present": self.evidence_query_product_replay_semantic_linkage_sample_replay_present,
            "replay_supported_event_count": self.evidence_query_product_replay_semantic_linkage_replay_supported_event_count,
            "aligned_rule_count": self.evidence_query_product_replay_semantic_linkage_aligned_rule_count,
            "pending_rule_count": self.evidence_query_product_replay_semantic_linkage_pending_rule_count,
            "correlation_count": self.evidence_query_product_replay_semantic_linkage_correlation_count,
            "ready_correlation_count": self.evidence_query_product_replay_semantic_linkage_ready_correlation_count,
            "blocked_correlation_count": self.evidence_query_product_replay_semantic_linkage_blocked_correlation_count,
            "blocker_count": self.evidence_query_product_replay_semantic_linkage_blocker_count,
            "target_or_validation_action_count": self.evidence_query_product_replay_semantic_linkage_target_or_validation_action_count,
            "first_blocker": self.evidence_query_product_replay_semantic_linkage_first_blocker,
            "next_action": self.evidence_query_product_replay_semantic_linkage_next_action,
        }


def project_release_pipeline_product_replay(
    release_pipeline: Mapping[str, Any],
    cross_artifact_review: Mapping[str, Any],
    semantic_linkage_review: Mapping[str, Any],
    evidence_query_summary: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any],
    truthy_coercer: Callable[[Any], Any],
    preserve_missing: bool = False,
) -> ReleasePipelineProductReplayProjection:
    """Resolve Product Replay release fields with one canonical source order."""
    pipeline = _mapping_value(release_pipeline)
    cross_review = _mapping_value(cross_artifact_review)
    linkage_review = _mapping_value(semantic_linkage_review)
    query_summary = _mapping_value(evidence_query_summary)

    projection_default = partial(
        missing_preserving_default,
        preserve_missing=preserve_missing,
    )

    def family_value(suffix: str, default: Any = None) -> Any:
        return _first_non_none(
            pipeline.get(
                f"evidence_query_cross_artifact_product_replay_family_{suffix}"
            ),
            cross_review.get(f"product_replay_family_{suffix}"),
            query_summary.get(f"cross_artifact_product_replay_family_{suffix}"),
            default,
        )

    def linkage_value(
        suffix: str,
        review_suffix: Optional[str] = None,
        default: Any = None,
        summary_key: Optional[str] = None,
        pipeline_key: Optional[str] = None,
    ) -> Any:
        return _first_non_none(
            pipeline.get(
                pipeline_key
                or f"evidence_query_product_replay_semantic_linkage_{suffix}"
            ),
            linkage_review.get(review_suffix or suffix),
            query_summary.get(
                summary_key or f"product_replay_semantic_linkage_{suffix}"
            ),
            default,
        )

    return ReleasePipelineProductReplayProjection(
        evidence_query_cross_artifact_product_replay_family_present=truthy_coercer(
            family_value("present", projection_default(False))
        ),
        evidence_query_cross_artifact_product_replay_family_status=family_value(
            "status", projection_default("not_reported")
        ),
        evidence_query_cross_artifact_product_replay_family_blocker_count=count_coercer(
            family_value("blocker_count")
        ),
        evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count=count_coercer(
            family_value("semantic_linkage_blocker_count")
        ),
        evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count=count_coercer(
            family_value("replay_or_execute_missing_count")
        ),
        evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count=count_coercer(
            family_value("target_or_validation_action_count")
        ),
        evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count=count_coercer(
            family_value("target_product_replay_action_count")
        ),
        evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count=count_coercer(
            family_value("target_gpu_execute_action_count")
        ),
        evidence_query_cross_artifact_product_replay_family_source_artifact_count=count_coercer(
            family_value("source_artifact_count")
        ),
        evidence_query_cross_artifact_product_replay_family_first_source_artifact=family_value(
            "first_source_artifact", projection_default("unknown")
        ),
        evidence_query_cross_artifact_product_replay_family_next_action=family_value(
            "next_action", projection_default("unknown")
        ),
        evidence_query_product_replay_semantic_linkage_present=truthy_coercer(
            linkage_value(
                "present",
                "linkage_present",
                projection_default(False),
            )
        ),
        evidence_query_product_replay_semantic_linkage_status=linkage_value(
            "status", default=projection_default("unknown")
        ),
        evidence_query_product_replay_semantic_events_present=truthy_coercer(
            linkage_value(
                "semantic_events_present",
                default=projection_default(False),
                summary_key="product_replay_semantic_events_present",
                pipeline_key="evidence_query_product_replay_semantic_events_present",
            )
        ),
        evidence_query_product_replay_alignment_present=truthy_coercer(
            linkage_value(
                "alignment_present",
                default=projection_default(False),
                summary_key="product_replay_alignment_present",
                pipeline_key="evidence_query_product_replay_alignment_present",
            )
        ),
        evidence_query_product_replay_semantic_linkage_read_issue_count=count_coercer(
            linkage_value("read_issue_count")
        ),
        evidence_query_product_replay_semantic_linkage_pending_event_count=count_coercer(
            linkage_value("pending_event_count")
        ),
        evidence_query_product_replay_semantic_linkage_sample_event_count=count_coercer(
            linkage_value("sample_event_count")
        ),
        evidence_query_product_replay_semantic_linkage_sample_replay_present=truthy_coercer(
            linkage_value("sample_replay_present", default=projection_default(False))
        ),
        evidence_query_product_replay_semantic_linkage_replay_supported_event_count=count_coercer(
            linkage_value("replay_supported_event_count")
        ),
        evidence_query_product_replay_semantic_linkage_aligned_rule_count=count_coercer(
            linkage_value("aligned_rule_count")
        ),
        evidence_query_product_replay_semantic_linkage_pending_rule_count=count_coercer(
            linkage_value("pending_rule_count")
        ),
        evidence_query_product_replay_semantic_linkage_correlation_count=count_coercer(
            linkage_value("correlation_count")
        ),
        evidence_query_product_replay_semantic_linkage_ready_correlation_count=count_coercer(
            linkage_value("ready_correlation_count")
        ),
        evidence_query_product_replay_semantic_linkage_blocked_correlation_count=count_coercer(
            linkage_value("blocked_correlation_count")
        ),
        evidence_query_product_replay_semantic_linkage_blocker_count=count_coercer(
            linkage_value("blocker_count")
        ),
        evidence_query_product_replay_semantic_linkage_target_or_validation_action_count=count_coercer(
            linkage_value("target_or_validation_action_count")
        ),
        evidence_query_product_replay_semantic_linkage_first_blocker=linkage_value(
            "first_blocker", default=projection_default("")
        ),
        evidence_query_product_replay_semantic_linkage_next_action=linkage_value(
            "next_action", default=projection_default("")
        ),
        product_replay_semantic_linkage_repair_path_count=count_coercer(
            pipeline.get("product_replay_semantic_linkage_repair_path_count")
        ),
        product_replay_semantic_linkage_repair_path_source_location_gap_count=count_coercer(
            pipeline.get(
                "product_replay_semantic_linkage_repair_path_source_location_gap_count"
            )
        ),
        evidence_query_product_replay_semantic_linkage_repair_path_count=count_coercer(
            linkage_value("repair_path_count", "release_repair_path_count")
        ),
        evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count=count_coercer(
            linkage_value(
                "repair_path_source_location_gap_count",
                "release_repair_path_source_location_gap_count",
            )
        ),
    )


@dataclass(frozen=True)
class RootCauseProductReplayProjection:
    product_replay_semantic_linkage_root_cause_count: int = 0
    product_replay_semantic_linkage_status: Any = "not_reported"
    product_replay_semantic_linkage_blocker_count: int = 0
    product_replay_semantic_linkage_pending_event_count: int = 0
    product_replay_semantic_linkage_pending_rule_count: int = 0
    product_replay_semantic_linkage_blocked_correlation_count: int = 0
    product_replay_semantic_linkage_target_or_validation_action_count: int = 0
    product_replay_semantic_linkage_first_blocker: Any = ""
    product_replay_semantic_linkage_next_action: Any = ""
    product_replay_semantic_linkage_blocks_release: bool = False
    product_replay_semantic_linkage_repair_path_count: int = 0
    product_replay_semantic_linkage_repair_path_source_location_gap_count: int = 0
    product_replay_family_gate_root_cause_count: int = 0
    product_replay_family_gate_status: Any = "not_reported"
    product_replay_family_gate_blocker_count: int = 0
    product_replay_family_gate_semantic_linkage_blocker_count: int = 0
    product_replay_family_gate_replay_or_execute_missing_count: int = 0
    product_replay_family_gate_target_or_validation_action_count: int = 0
    product_replay_family_gate_target_product_replay_action_count: int = 0
    product_replay_family_gate_target_gpu_execute_action_count: int = 0
    product_replay_family_gate_typed_environment_split_present: bool = False
    product_replay_family_gate_source_artifact_count: int = 0
    product_replay_family_gate_first_source_artifact: Any = ""
    product_replay_family_gate_next_action: Any = ""
    product_replay_family_gate_blocks_release: bool = False
    product_replay_family_gate_repair_path_count: int = 0
    product_replay_family_gate_repair_path_source_location_gap_count: int = 0

    def to_root_cause_review_dict(self) -> Dict[str, Any]:
        return {
            "product_replay_semantic_linkage_root_cause_count": self.product_replay_semantic_linkage_root_cause_count,
            "product_replay_semantic_linkage_status": self.product_replay_semantic_linkage_status,
            "product_replay_semantic_linkage_blocker_count": self.product_replay_semantic_linkage_blocker_count,
            "product_replay_semantic_linkage_pending_event_count": self.product_replay_semantic_linkage_pending_event_count,
            "product_replay_semantic_linkage_pending_rule_count": self.product_replay_semantic_linkage_pending_rule_count,
            "product_replay_semantic_linkage_blocked_correlation_count": self.product_replay_semantic_linkage_blocked_correlation_count,
            "product_replay_semantic_linkage_target_or_validation_action_count": self.product_replay_semantic_linkage_target_or_validation_action_count,
            "product_replay_semantic_linkage_first_blocker": self.product_replay_semantic_linkage_first_blocker,
            "product_replay_semantic_linkage_next_action": self.product_replay_semantic_linkage_next_action,
            "product_replay_semantic_linkage_blocks_release": self.product_replay_semantic_linkage_blocks_release,
            "product_replay_family_gate_root_cause_count": self.product_replay_family_gate_root_cause_count,
            "product_replay_family_gate_status": self.product_replay_family_gate_status,
            "product_replay_family_gate_blocker_count": self.product_replay_family_gate_blocker_count,
            "product_replay_family_gate_semantic_linkage_blocker_count": self.product_replay_family_gate_semantic_linkage_blocker_count,
            "product_replay_family_gate_replay_or_execute_missing_count": self.product_replay_family_gate_replay_or_execute_missing_count,
            "product_replay_family_gate_target_or_validation_action_count": self.product_replay_family_gate_target_or_validation_action_count,
            "product_replay_family_gate_target_product_replay_action_count": self.product_replay_family_gate_target_product_replay_action_count,
            "product_replay_family_gate_target_gpu_execute_action_count": self.product_replay_family_gate_target_gpu_execute_action_count,
            "product_replay_family_gate_typed_environment_split_present": self.product_replay_family_gate_typed_environment_split_present,
            "product_replay_family_gate_source_artifact_count": self.product_replay_family_gate_source_artifact_count,
            "product_replay_family_gate_first_source_artifact": self.product_replay_family_gate_first_source_artifact,
            "product_replay_family_gate_next_action": self.product_replay_family_gate_next_action,
            "product_replay_family_gate_blocks_release": self.product_replay_family_gate_blocks_release,
        }

    def to_final_report_scorecard_dict(self) -> Dict[str, Any]:
        return {
            "root_cause_product_replay_semantic_linkage_root_cause_count": self.product_replay_semantic_linkage_root_cause_count,
            "root_cause_product_replay_semantic_linkage_status": self.product_replay_semantic_linkage_status,
            "root_cause_product_replay_semantic_linkage_blocker_count": self.product_replay_semantic_linkage_blocker_count,
            "root_cause_product_replay_semantic_linkage_pending_event_count": self.product_replay_semantic_linkage_pending_event_count,
            "root_cause_product_replay_semantic_linkage_pending_rule_count": self.product_replay_semantic_linkage_pending_rule_count,
            "root_cause_product_replay_semantic_linkage_blocked_correlation_count": self.product_replay_semantic_linkage_blocked_correlation_count,
            "root_cause_product_replay_semantic_linkage_target_or_validation_action_count": self.product_replay_semantic_linkage_target_or_validation_action_count,
            "root_cause_product_replay_semantic_linkage_first_blocker": self.product_replay_semantic_linkage_first_blocker,
            "root_cause_product_replay_semantic_linkage_next_action": self.product_replay_semantic_linkage_next_action,
            "root_cause_product_replay_semantic_linkage_blocks_release": self.product_replay_semantic_linkage_blocks_release,
            "root_cause_product_replay_semantic_linkage_repair_path_count": self.product_replay_semantic_linkage_repair_path_count,
            "root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count": self.product_replay_semantic_linkage_repair_path_source_location_gap_count,
        }

    def to_final_report_blocker_counts_dict(self) -> Dict[str, int]:
        return {
            "release_pipeline_root_cause_product_replay_semantic_linkage_repair_path_count": self.product_replay_semantic_linkage_repair_path_count,
            "release_pipeline_root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count": self.product_replay_semantic_linkage_repair_path_source_location_gap_count,
            "release_pipeline_root_cause_product_replay_family_gate_repair_path_count": self.product_replay_family_gate_repair_path_count,
            "release_pipeline_root_cause_product_replay_family_gate_repair_path_source_location_gap_count": self.product_replay_family_gate_repair_path_source_location_gap_count,
        }


def project_root_cause_product_replay(
    release_pipeline: Mapping[str, Any],
    root_cause_review: Mapping[str, Any],
    root_cause_summary: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any],
    truthy_coercer: Callable[[Any], Any],
    preserve_missing: bool = False,
) -> RootCauseProductReplayProjection:
    """Resolve Product Replay root-cause fields with one canonical source order."""
    pipeline = _mapping_value(release_pipeline)
    review = _mapping_value(root_cause_review)
    artifact_summary = _mapping_value(root_cause_summary)

    projection_default = partial(
        missing_preserving_default,
        preserve_missing=preserve_missing,
    )

    def root_value(
        family: str,
        suffix: str,
        default: Any = None,
    ) -> Any:
        return _first_non_none(
            review.get(f"{family}_{suffix}"),
            artifact_summary.get(f"{family}_{suffix}"),
            default,
        )

    return RootCauseProductReplayProjection(
        product_replay_semantic_linkage_root_cause_count=count_coercer(
            root_value("product_replay_semantic_linkage", "root_cause_count")
        ),
        product_replay_semantic_linkage_status=root_value(
            "product_replay_semantic_linkage",
            "status",
            projection_default("not_reported"),
        ),
        product_replay_semantic_linkage_blocker_count=count_coercer(
            root_value("product_replay_semantic_linkage", "blocker_count")
        ),
        product_replay_semantic_linkage_pending_event_count=count_coercer(
            root_value("product_replay_semantic_linkage", "pending_event_count")
        ),
        product_replay_semantic_linkage_pending_rule_count=count_coercer(
            root_value("product_replay_semantic_linkage", "pending_rule_count")
        ),
        product_replay_semantic_linkage_blocked_correlation_count=count_coercer(
            root_value("product_replay_semantic_linkage", "blocked_correlation_count")
        ),
        product_replay_semantic_linkage_target_or_validation_action_count=count_coercer(
            root_value(
                "product_replay_semantic_linkage",
                "target_or_validation_action_count",
            )
        ),
        product_replay_semantic_linkage_first_blocker=root_value(
            "product_replay_semantic_linkage",
            "first_blocker",
            projection_default(""),
        ),
        product_replay_semantic_linkage_next_action=root_value(
            "product_replay_semantic_linkage",
            "next_action",
            projection_default(""),
        ),
        product_replay_semantic_linkage_blocks_release=truthy_coercer(
            root_value(
                "product_replay_semantic_linkage",
                "blocks_release",
                projection_default(False),
            )
        ),
        product_replay_semantic_linkage_repair_path_count=count_coercer(
            _first_non_none(
                pipeline.get(
                    "root_cause_product_replay_semantic_linkage_repair_path_count"
                ),
                review.get("release_repair_path_count"),
            )
        ),
        product_replay_semantic_linkage_repair_path_source_location_gap_count=count_coercer(
            _first_non_none(
                pipeline.get(
                    "root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count"
                ),
                review.get("release_repair_path_source_location_gap_count"),
            )
        ),
        product_replay_family_gate_root_cause_count=count_coercer(
            root_value("product_replay_family_gate", "root_cause_count")
        ),
        product_replay_family_gate_status=root_value(
            "product_replay_family_gate",
            "status",
            projection_default("not_reported"),
        ),
        product_replay_family_gate_blocker_count=count_coercer(
            root_value("product_replay_family_gate", "blocker_count")
        ),
        product_replay_family_gate_semantic_linkage_blocker_count=count_coercer(
            root_value(
                "product_replay_family_gate",
                "semantic_linkage_blocker_count",
            )
        ),
        product_replay_family_gate_replay_or_execute_missing_count=count_coercer(
            root_value(
                "product_replay_family_gate",
                "replay_or_execute_missing_count",
            )
        ),
        product_replay_family_gate_target_or_validation_action_count=count_coercer(
            root_value(
                "product_replay_family_gate",
                "target_or_validation_action_count",
            )
        ),
        product_replay_family_gate_target_product_replay_action_count=count_coercer(
            root_value(
                "product_replay_family_gate",
                "target_product_replay_action_count",
            )
        ),
        product_replay_family_gate_target_gpu_execute_action_count=count_coercer(
            root_value(
                "product_replay_family_gate",
                "target_gpu_execute_action_count",
            )
        ),
        product_replay_family_gate_typed_environment_split_present=truthy_coercer(
            root_value(
                "product_replay_family_gate",
                "typed_environment_split_present",
                projection_default(False),
            )
        ),
        product_replay_family_gate_source_artifact_count=count_coercer(
            root_value("product_replay_family_gate", "source_artifact_count")
        ),
        product_replay_family_gate_first_source_artifact=root_value(
            "product_replay_family_gate",
            "first_source_artifact",
            projection_default(""),
        ),
        product_replay_family_gate_next_action=root_value(
            "product_replay_family_gate",
            "next_action",
            projection_default(""),
        ),
        product_replay_family_gate_blocks_release=truthy_coercer(
            root_value(
                "product_replay_family_gate",
                "blocks_release",
                projection_default(False),
            )
        ),
        product_replay_family_gate_repair_path_count=count_coercer(
            _first_non_none(
                pipeline.get(
                    "root_cause_product_replay_family_gate_repair_path_count"
                ),
                review.get(
                    "root_cause_product_replay_family_gate_repair_path_count"
                ),
            )
        ),
        product_replay_family_gate_repair_path_source_location_gap_count=count_coercer(
            _first_non_none(
                pipeline.get(
                    "root_cause_product_replay_family_gate_repair_path_source_location_gap_count"
                ),
                review.get(
                    "root_cause_product_replay_family_gate_repair_path_source_location_gap_count"
                ),
            )
        ),
    )


_EVIDENCE_QUERY_UX_READINESS_PATH_FIELDS = (
    "readiness_path_target",
    "readiness_path_state",
    "readiness_path_blocking_repair_card_count",
    "readiness_path_step_count",
    "readiness_path_current_repo_step_count",
    "readiness_path_target_or_validation_step_count",
    "readiness_path_source_location_gap_count",
    "readiness_path_catalog_flow_discrepancy_count",
    "readiness_path_fallback_removal_blocked_count",
    "readiness_path_field_coverage_gap_count",
    "readiness_path_actionability_status",
    "readiness_path_primary_environment",
    "readiness_path_blocker_reason",
    "readiness_path_current_repo_actionable",
    "readiness_path_validation_or_target_required",
    "readiness_path_source_location_metadata_required",
    "readiness_path_next_action_present",
)

_EVIDENCE_QUERY_UX_BLOCKER_DRILLDOWN_FIELDS = (
    "blocker_drilldown_record_count",
    "blocker_drilldown_category_count",
    "blocker_drilldown_source_location_gap_count",
    "blocker_drilldown_affected_claim_count",
    "blocker_drilldown_exact_wiring_location_count",
    "blocker_drilldown_current_repo_action_count",
    "blocker_drilldown_current_server_action_count",
    "blocker_drilldown_target_machine_action_count",
    "blocker_drilldown_target_product_action_count",
)

_EVIDENCE_QUERY_UX_SOURCE_EXACTNESS_FIELDS = (
    "repair_card_source_exactness_card_count",
    "repair_card_source_exactness_exact_card_count",
    "repair_card_source_exactness_gap_card_count",
    "repair_card_source_exactness_current_repo_gap_count",
    "repair_card_source_exactness_target_or_validation_gap_count",
    "repair_card_source_exactness_percent",
    "explain_tree_node_count",
    "explain_tree_source_exact_node_count",
    "explain_tree_source_gap_node_count",
    "explain_tree_current_repo_source_gap_count",
    "explain_tree_target_or_validation_source_gap_count",
    "explain_tree_source_exactness_percent",
)

_EVIDENCE_QUERY_UX_CROSS_ARTIFACT_FIELDS = (
    "cross_artifact_blocker_explanation_status",
    "cross_artifact_blocker_family_count",
    "cross_artifact_active_family_count",
    "cross_artifact_total_blocker_count",
    "cross_artifact_current_repo_action_count",
    "cross_artifact_target_or_validation_action_count",
    "cross_artifact_source_location_gap_count",
    "cross_artifact_primary_family",
    "cross_artifact_primary_blocker_reason",
    "cross_artifact_primary_environment",
    "cross_artifact_release_focus_decision",
    "cross_artifact_release_focus_repair_lane",
    "cross_artifact_release_focus_affected_claim_count",
    "cross_artifact_release_focus_repair_path_count",
    "cross_artifact_release_focus_first_repair_family",
    "cross_artifact_release_focus_first_repair_environment",
    "cross_artifact_release_focus_first_repair_action",
)

_evidence_query_ux_summary_keys = tuple(
    f"query_ux_{name}" for name in _EVIDENCE_QUERY_UX_READINESS_PATH_FIELDS
) + tuple(
    f"query_ux_{name}"
    for name in (
        _EVIDENCE_QUERY_UX_BLOCKER_DRILLDOWN_FIELDS
        + _EVIDENCE_QUERY_UX_SOURCE_EXACTNESS_FIELDS
    )
) + _EVIDENCE_QUERY_UX_CROSS_ARTIFACT_FIELDS
if len(_evidence_query_ux_summary_keys) != len(set(_evidence_query_ux_summary_keys)):
    raise ValueError("evidence-query UX summary keys must be unique")


def project_evidence_query_ux_summary(
    evidence_query_ux: Optional[Mapping[str, Any]],
) -> Dict[str, Any]:
    """Project the owning Evidence Query UX object into legacy summary keys."""
    ux = _mapping_value(evidence_query_ux)
    projection = {
        f"query_ux_{name}": ux.get(name)
        for name in (
            _EVIDENCE_QUERY_UX_READINESS_PATH_FIELDS
            + _EVIDENCE_QUERY_UX_BLOCKER_DRILLDOWN_FIELDS
            + _EVIDENCE_QUERY_UX_SOURCE_EXACTNESS_FIELDS
        )
    }
    projection.update(
        {
            name: ux.get(name)
            for name in _EVIDENCE_QUERY_UX_CROSS_ARTIFACT_FIELDS
        }
    )
    return projection


def project_artifact_catalog_governance(
    data: Optional[Mapping[str, Any]],
) -> Dict[str, Any]:
    """Project one catalog artifact into the canonical downstream review model."""
    document = _mapping_value(data)
    summary = _mapping_value(document.get("summary"))
    drift = _mapping_value(document.get("catalog_drift_review"))
    parity = _mapping_value(document.get("single_source_parity_review"))
    metadata = _mapping_value(document.get("metadata_completeness_review"))
    migration = _mapping_value(document.get("migration_readiness"))
    fallback_queue = _mapping_value(document.get("fallback_removal_queue"))
    fallback_shortest_path = _mapping_value(
        document.get("fallback_removal_shortest_path")
    )
    fallback_parity_gate = _mapping_value(
        document.get("fallback_removal_parity_gate")
    )

    projection = {
        "source": "native_artifact_catalog_single_source.json",
        "present": data is not None,
        "registry_decision": summary.get("registry_decision"),
        "registered_artifact_count": summary.get("registered_artifact_count"),
        "registry_consumer_surface_count": summary.get(
            "registry_consumer_surface_count"
        ),
        "registry_consumed_surface_count": summary.get(
            "registry_consumed_surface_count"
        ),
        "hardcoded_duplicate_location_count": summary.get(
            "hardcoded_duplicate_location_count"
        ),
        "duplicate_registration_count": summary.get(
            "duplicate_registration_count"
        ),
        "missing_wiring_policy_count": summary.get("missing_wiring_policy_count"),
        "downstream_wiring_matrix_entry_count": summary.get(
            "downstream_wiring_matrix_entry_count"
        ),
        "downstream_wiring_surface_count": summary.get(
            "downstream_wiring_surface_count"
        ),
        "catalog_drift_missing_surface_count": summary.get(
            "catalog_drift_missing_surface_count"
        ),
        "catalog_fallback_retained_surface_count": summary.get(
            "catalog_fallback_retained_surface_count"
        ),
        "catalog_surface_migration_ready_count": summary.get(
            "catalog_surface_migration_ready_count"
        ),
        "catalog_surface_migration_fallback_count": summary.get(
            "catalog_surface_migration_fallback_count"
        ),
        "catalog_surface_parity_refresh_required_count": summary.get(
            "catalog_surface_parity_refresh_required_count"
        ),
        "catalog_surface_current_repo_action_count": summary.get(
            "catalog_surface_current_repo_action_count"
        ),
        "catalog_surface_target_machine_action_count": summary.get(
            "catalog_surface_target_machine_action_count"
        ),
        "single_source_repair_card_count": summary.get(
            "single_source_repair_card_count"
        ),
        "single_source_fallback_repair_card_count": summary.get(
            "single_source_fallback_repair_card_count"
        ),
        "single_source_parity_refresh_required_count": summary.get(
            "single_source_parity_refresh_required_count"
        ),
        "surface_consumer_migration_plan_count": _first_non_none(
            summary.get("surface_consumer_migration_plan_count"),
            migration.get("surface_consumer_migration_plan_count"),
        ),
        "surface_consumer_fallback_removal_blocked_count": _first_non_none(
            summary.get("surface_consumer_fallback_removal_blocked_count"),
            migration.get("surface_consumer_fallback_removal_blocked_count"),
        ),
        "surface_consumer_parity_refresh_required_count": _first_non_none(
            summary.get("surface_consumer_parity_refresh_required_count"),
            migration.get("surface_consumer_parity_refresh_required_count"),
        ),
        "surface_consumer_current_repo_action_count": _first_non_none(
            summary.get("surface_consumer_current_repo_action_count"),
            migration.get("surface_consumer_current_repo_action_count"),
        ),
        "surface_consumer_target_machine_action_count": _first_non_none(
            summary.get("surface_consumer_target_machine_action_count"),
            migration.get("surface_consumer_target_machine_action_count"),
        ),
        "fallback_removal_queue_item_count": _first_non_none(
            summary.get("fallback_removal_queue_item_count"),
            fallback_queue.get("queue_item_count"),
        ),
        "fallback_removal_queue_blocked_count": _first_non_none(
            summary.get("fallback_removal_queue_blocked_count"),
            fallback_queue.get("fallback_removal_blocked_count"),
        ),
        "fallback_removal_queue_parity_refresh_required_count": _first_non_none(
            summary.get("fallback_removal_queue_parity_refresh_required_count"),
            fallback_queue.get("parity_refresh_required_count"),
        ),
        "fallback_removal_queue_current_repo_action_count": _first_non_none(
            summary.get("fallback_removal_queue_current_repo_action_count"),
            fallback_queue.get("current_repo_action_count"),
        ),
        "fallback_removal_queue_target_machine_action_count": _first_non_none(
            summary.get("fallback_removal_queue_target_machine_action_count"),
            fallback_queue.get("target_machine_action_count"),
        ),
        "fallback_removal_queue_state": _first_non_none(
            summary.get("fallback_removal_queue_state"),
            fallback_queue.get("queue_state"),
        ),
        "fallback_removal_shortest_path_present": _first_non_none(
            summary.get("fallback_removal_shortest_path_present"),
            fallback_shortest_path.get("present"),
        ),
        "fallback_removal_shortest_path_state": _first_non_none(
            summary.get("fallback_removal_shortest_path_state"),
            fallback_shortest_path.get("state"),
        ),
        "fallback_removal_shortest_path_step_count": _first_non_none(
            summary.get("fallback_removal_shortest_path_step_count"),
            fallback_shortest_path.get("step_count"),
        ),
        "fallback_removal_shortest_path_current_repo_step_count": _first_non_none(
            summary.get("fallback_removal_shortest_path_current_repo_step_count"),
            fallback_shortest_path.get("current_repo_step_count"),
        ),
        "fallback_removal_shortest_path_target_machine_step_count": _first_non_none(
            summary.get("fallback_removal_shortest_path_target_machine_step_count"),
            fallback_shortest_path.get("target_machine_step_count"),
        ),
        "fallback_removal_shortest_path_source_location_gap_count": _first_non_none(
            summary.get("fallback_removal_shortest_path_source_location_gap_count"),
            fallback_shortest_path.get("source_location_gap_count"),
        ),
        "fallback_removal_shortest_path_primary_environment": _first_non_none(
            summary.get("fallback_removal_shortest_path_primary_environment"),
            fallback_shortest_path.get("primary_environment"),
        ),
        "fallback_removal_shortest_path_first_surface": _first_non_none(
            summary.get("fallback_removal_shortest_path_first_surface"),
            fallback_shortest_path.get("first_surface"),
        ),
        "fallback_removal_parity_gate_present": _first_non_none(
            summary.get("fallback_removal_parity_gate_present"),
            fallback_parity_gate.get("present"),
        ),
        "fallback_removal_parity_gate_decision": _first_non_none(
            summary.get("fallback_removal_parity_gate_decision"),
            fallback_parity_gate.get("decision"),
        ),
        "fallback_removal_parity_gate_removal_allowed": _first_non_none(
            summary.get("fallback_removal_parity_gate_removal_allowed"),
            fallback_parity_gate.get("removal_allowed"),
        ),
        "fallback_removal_parity_gate_blocker_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_blocker_count"),
            fallback_parity_gate.get("blocker_count"),
        ),
        "fallback_removal_parity_gate_blocking_surface_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_blocking_surface_count"),
            fallback_parity_gate.get("blocking_surface_count"),
        ),
        "fallback_removal_parity_gate_fallback_surface_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_fallback_surface_count"),
            fallback_parity_gate.get("fallback_surface_count"),
        ),
        "fallback_removal_parity_gate_parity_refresh_required_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_parity_refresh_required_count"),
            fallback_parity_gate.get("parity_refresh_required_count"),
        ),
        "fallback_removal_parity_gate_current_repo_action_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_current_repo_action_count"),
            fallback_parity_gate.get("current_repo_action_count"),
        ),
        "fallback_removal_parity_gate_target_machine_action_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_target_machine_action_count"),
            fallback_parity_gate.get("target_machine_action_count"),
        ),
        "fallback_removal_parity_gate_source_location_gap_count": _first_non_none(
            summary.get("fallback_removal_parity_gate_source_location_gap_count"),
            fallback_parity_gate.get("source_location_gap_count"),
        ),
        "fallback_removal_parity_gate_blocking_surface_source_location_gap_count": _first_non_none(
            summary.get(
                "fallback_removal_parity_gate_blocking_surface_source_location_gap_count"
            ),
            fallback_parity_gate.get("blocking_surface_source_location_gap_count"),
        ),
        "fallback_removal_parity_gate_first_surface": _first_non_none(
            summary.get("fallback_removal_parity_gate_first_surface"),
            fallback_parity_gate.get("first_blocking_surface"),
        ),
        "consumer_field_coverage_surface_count": _first_non_none(
            summary.get("consumer_field_coverage_surface_count"),
            migration.get("consumer_field_coverage_surface_count"),
        ),
        "consumer_field_coverage_catalog_managed_surface_count": _first_non_none(
            summary.get("consumer_field_coverage_catalog_managed_surface_count"),
            migration.get("consumer_field_coverage_catalog_managed_surface_count"),
        ),
        "consumer_field_coverage_fallback_surface_count": _first_non_none(
            summary.get("consumer_field_coverage_fallback_surface_count"),
            migration.get("consumer_field_coverage_fallback_surface_count"),
        ),
        "consumer_field_coverage_required_field_count": summary.get(
            "consumer_field_coverage_required_field_count"
        ),
        "consumer_field_coverage_covered_field_count": summary.get(
            "consumer_field_coverage_covered_field_count"
        ),
        "consumer_field_coverage_field_gap_count": _first_non_none(
            summary.get("consumer_field_coverage_field_gap_count"),
            migration.get("consumer_field_coverage_field_gap_count"),
        ),
        "consumer_field_coverage_parity_refresh_required_count": _first_non_none(
            summary.get("consumer_field_coverage_parity_refresh_required_count"),
            migration.get("consumer_field_coverage_parity_refresh_required_count"),
        ),
        "consumer_field_coverage_current_repo_action_count": summary.get(
            "consumer_field_coverage_current_repo_action_count"
        ),
        "consumer_field_coverage_target_machine_action_count": summary.get(
            "consumer_field_coverage_target_machine_action_count"
        ),
        "registry_explicit_policy_complete_artifact_count": summary.get(
            "registry_explicit_policy_complete_artifact_count"
        ),
        "registry_derived_policy_artifact_count": summary.get(
            "registry_derived_policy_artifact_count"
        ),
        "registry_derived_policy_field_count": summary.get(
            "registry_derived_policy_field_count"
        ),
        "registry_metadata_policy_card_count": summary.get(
            "registry_metadata_policy_card_count"
        ),
        "drift_decision": drift.get("drift_decision"),
        "fallback_retained_matrix_entry_count": drift.get(
            "fallback_retained_matrix_entry_count"
        ),
        "parity_decision": parity.get("parity_decision"),
        "parity_repair_card_count": parity.get("repair_card_count"),
        "parity_current_repo_action_count": parity.get("current_repo_action_count"),
        "metadata_decision": metadata.get("metadata_decision"),
        "metadata_policy_card_count": metadata.get("metadata_policy_card_count"),
        "metadata_derived_policy_artifact_count": metadata.get(
            "derived_policy_artifact_count"
        ),
        "metadata_derived_policy_field_count": metadata.get("derived_policy_field_count"),
        "catalog_drift_matrix_entry_count": drift.get("matrix_entry_count"),
        "catalog_drift_registry_entry_count": drift.get("registry_entry_count"),
        "catalog_drift_fallback_retained_matrix_entry_count": drift.get(
            "fallback_retained_matrix_entry_count"
        ),
        "single_source_parity_repair_card_count": parity.get("repair_card_count"),
        "single_source_parity_current_repo_action_count": parity.get(
            "current_repo_action_count"
        ),
        "single_source_parity_review_refresh_required_count": parity.get(
            "parity_refresh_required_count"
        ),
        "metadata_review_derived_policy_artifact_count": metadata.get(
            "derived_policy_artifact_count"
        ),
        "metadata_review_derived_policy_field_count": metadata.get(
            "derived_policy_field_count"
        ),
        "metadata_review_policy_card_count": metadata.get(
            "metadata_policy_card_count"
        ),
        "claim_boundary": "Catalog drift review is source-registration evidence only; it does not prove downstream artifact freshness, execute, replay, or benchmark results.",
    }
    return projection


@dataclass(frozen=True)
class DomainFieldSpec:
    name: str
    value_kind: str = DOMAIN_VALUE_RAW
    default: Any = None
    source_name: Optional[str] = None


ARTIFACT_CATALOG_PROMETHEUS_METRIC_NAMES = (
    "registered_artifact_count",
    "registry_consumer_surface_count",
    "registry_consumed_surface_count",
    "hardcoded_duplicate_location_count",
    "duplicate_registration_count",
    "missing_wiring_policy_count",
    "downstream_wiring_matrix_entry_count",
    "downstream_wiring_surface_count",
    "catalog_drift_missing_surface_count",
    "catalog_fallback_retained_surface_count",
    "catalog_surface_migration_ready_count",
    "catalog_surface_migration_fallback_count",
    "catalog_surface_parity_refresh_required_count",
    "catalog_surface_current_repo_action_count",
    "catalog_surface_target_machine_action_count",
    "single_source_repair_card_count",
    "single_source_fallback_repair_card_count",
    "single_source_parity_refresh_required_count",
    "registry_explicit_policy_complete_artifact_count",
    "registry_derived_policy_artifact_count",
    "registry_derived_policy_field_count",
    "registry_metadata_policy_card_count",
    "catalog_drift_matrix_entry_count",
    "catalog_drift_registry_entry_count",
    "catalog_drift_fallback_retained_matrix_entry_count",
    "single_source_parity_repair_card_count",
    "single_source_parity_current_repo_action_count",
    "single_source_parity_review_refresh_required_count",
    "surface_consumer_migration_plan_count",
    "surface_consumer_fallback_removal_blocked_count",
    "surface_consumer_parity_refresh_required_count",
    "surface_consumer_current_repo_action_count",
    "surface_consumer_target_machine_action_count",
    "fallback_removal_queue_item_count",
    "fallback_removal_queue_blocked_count",
    "fallback_removal_queue_parity_refresh_required_count",
    "fallback_removal_queue_current_repo_action_count",
    "fallback_removal_queue_target_machine_action_count",
    "fallback_removal_shortest_path_step_count",
    "fallback_removal_shortest_path_current_repo_step_count",
    "fallback_removal_shortest_path_target_machine_step_count",
    "fallback_removal_shortest_path_source_location_gap_count",
    "fallback_removal_parity_gate_blocker_count",
    "fallback_removal_parity_gate_blocking_surface_count",
    "fallback_removal_parity_gate_fallback_surface_count",
    "fallback_removal_parity_gate_parity_refresh_required_count",
    "fallback_removal_parity_gate_current_repo_action_count",
    "fallback_removal_parity_gate_target_machine_action_count",
    "fallback_removal_parity_gate_source_location_gap_count",
    "fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
    "fallback_removal_parity_gate_removal_allowed",
    "consumer_field_coverage_surface_count",
    "consumer_field_coverage_catalog_managed_surface_count",
    "consumer_field_coverage_fallback_surface_count",
    "consumer_field_coverage_required_field_count",
    "consumer_field_coverage_covered_field_count",
    "consumer_field_coverage_field_gap_count",
    "consumer_field_coverage_parity_refresh_required_count",
    "consumer_field_coverage_current_repo_action_count",
    "consumer_field_coverage_target_machine_action_count",
    "metadata_review_derived_policy_artifact_count",
    "metadata_review_derived_policy_field_count",
    "metadata_review_policy_card_count",
)


if len(set(ARTIFACT_CATALOG_PROMETHEUS_METRIC_NAMES)) != len(
    ARTIFACT_CATALOG_PROMETHEUS_METRIC_NAMES
):
    raise ValueError("artifact catalog Prometheus metric names must be unique")


@dataclass(frozen=True)
class ReleasePipelineRepairPathScorecardValues:
    repair_path_source_location_gap_count: int = 0
    repair_path_evidence_counter_count: int = 0
    repair_path_group_count: int = 0
    repair_path_environment_group_count: int = 0
    repair_path_category_group_count: int = 0
    repair_path_current_repo_action_group_count: int = 0
    repair_path_target_or_validation_action_group_count: int = 0
    repair_path_group_plan_step_count: int = 0
    repair_path_group_plan_current_repo_step_count: int = 0
    repair_path_group_plan_target_or_validation_step_count: int = 0
    catalog_flow_repair_path_source_location_gap_count: int = 0
    catalog_flow_repair_path_evidence_counter_count: int = 0
    evidence_query_snapshot_diff_repair_path_source_location_gap_count: int = 0
    evidence_query_snapshot_diff_repair_path_evidence_counter_count: int = 0
    native_core_audit_repair_path_evidence_counter_count: int = 0
    native_core_audit_scope_repair_path_evidence_counter_count: int = 0
    evidence_query_cross_artifact_release_focus_repair_path_count: int = 0
    evidence_query_product_replay_semantic_linkage_repair_path_count: int = 0
    evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count: int = 0
    root_cause_product_replay_semantic_linkage_repair_path_count: int = 0
    root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count: int = 0
    root_cause_product_replay_family_gate_repair_path_count: int = 0
    root_cause_product_replay_family_gate_repair_path_source_location_gap_count: int = 0


@dataclass(frozen=True)
class ReleasePipelineLinkageScorecardValues:
    evidence_query_snapshot_to_release_link_count: int = 0
    evidence_query_snapshot_to_release_blocker_count: int = 0
    evidence_query_snapshot_to_release_group_plan_step: int = 0
    evidence_query_snapshot_to_release_current_repo_action_count: int = 0
    blocker_repair_card_link_count: int = 0
    blocker_repair_card_linked_count: int = 0
    blocker_repair_card_unmatched_release_path_count: int = 0
    blocker_repair_card_unmatched_root_cause_count: int = 0
    blocker_repair_card_current_repo_action_count: int = 0
    blocker_repair_card_target_or_validation_action_count: int = 0


@dataclass(frozen=True)
class FinalReportReleaseLinkageProjection:
    snapshot_to_release_link_count: Any
    snapshot_to_release_blocker_count: Any
    snapshot_to_release_group_plan_step: Any
    snapshot_to_release_current_repo_action_count: Any
    snapshot_to_release_link_status: Any
    snapshot_to_release_blocker_reason: Any
    snapshot_to_release_linked_repair_path_id: Any
    blocker_repair_card_link_count: Any
    blocker_repair_card_linked_count: Any
    blocker_repair_card_unmatched_release_path_count: Any
    blocker_repair_card_unmatched_root_cause_count: Any
    blocker_repair_card_current_repo_action_count: Any
    blocker_repair_card_target_or_validation_action_count: Any
    blocker_repair_card_link_status: Any
    blocker_repair_card_root_cause_repair_card_count: Any
    blocker_repair_card_first_release_repair_path_id: Any
    blocker_repair_card_first_release_repair_path_category: Any
    blocker_repair_card_first_release_repair_source_location: Any

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return {
            "evidence_query_snapshot_to_release_link_count": (
                self.snapshot_to_release_link_count
            ),
            "evidence_query_snapshot_to_release_blocker_count": (
                self.snapshot_to_release_blocker_count
            ),
            "evidence_query_snapshot_to_release_group_plan_step": (
                self.snapshot_to_release_group_plan_step
            ),
            "evidence_query_snapshot_to_release_current_repo_action_count": (
                self.snapshot_to_release_current_repo_action_count
            ),
            "evidence_query_snapshot_to_release_link_status": (
                self.snapshot_to_release_link_status
            ),
            "evidence_query_snapshot_to_release_blocker_reason": (
                self.snapshot_to_release_blocker_reason
            ),
            "evidence_query_snapshot_to_release_linked_repair_path_id": (
                self.snapshot_to_release_linked_repair_path_id
            ),
            "blocker_repair_card_link_count": (
                self.blocker_repair_card_link_count
            ),
            "blocker_repair_card_linked_count": (
                self.blocker_repair_card_linked_count
            ),
            "blocker_repair_card_unmatched_release_path_count": (
                self.blocker_repair_card_unmatched_release_path_count
            ),
            "blocker_repair_card_unmatched_root_cause_count": (
                self.blocker_repair_card_unmatched_root_cause_count
            ),
            "blocker_repair_card_current_repo_action_count": (
                self.blocker_repair_card_current_repo_action_count
            ),
            "blocker_repair_card_target_or_validation_action_count": (
                self.blocker_repair_card_target_or_validation_action_count
            ),
            "blocker_repair_card_link_status": (
                self.blocker_repair_card_link_status
            ),
            "blocker_repair_card_root_cause_repair_card_count": (
                self.blocker_repair_card_root_cause_repair_card_count
            ),
            "blocker_repair_card_first_release_repair_path_id": (
                self.blocker_repair_card_first_release_repair_path_id
            ),
            "blocker_repair_card_first_release_repair_path_category": (
                self.blocker_repair_card_first_release_repair_path_category
            ),
            "blocker_repair_card_first_release_repair_source_location": (
                self.blocker_repair_card_first_release_repair_source_location
            ),
        }

    def to_release_pipeline_scorecard_dict(
        self,
        *,
        count_coercer: Callable[[Any], int],
    ) -> Dict[str, Any]:
        scorecard = project_domain_fields(
            self.to_release_pipeline_dict(),
            RELEASE_PIPELINE_LINKAGE_SCORECARD_FIELDS,
            count_coercer=count_coercer,
        )
        scorecard["release_pipeline_blocker_repair_card_link_status"] = (
            self.blocker_repair_card_link_status
        )
        return scorecard

    def to_final_report_blocker_counts_dict(
        self,
        *,
        count_coercer: Callable[[Any], int],
    ) -> Dict[str, int]:
        return {
            "release_pipeline_blocker_repair_card_unmatched_release_paths": (
                count_coercer(
                    self.blocker_repair_card_unmatched_release_path_count
                )
            ),
            "release_pipeline_blocker_repair_card_unmatched_root_causes": (
                count_coercer(
                    self.blocker_repair_card_unmatched_root_cause_count
                )
            ),
        }


def project_final_report_release_linkage(
    release_summary: Mapping[str, Any],
    snapshot_to_release_link: Mapping[str, Any],
    blocker_repair_card_link: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportReleaseLinkageProjection:
    """Project release linkage mirrors with one canonical source order."""
    release = _mapping_value(release_summary)
    snapshot_link = _mapping_value(snapshot_to_release_link)
    blocker_link = _mapping_value(blocker_repair_card_link)
    return FinalReportReleaseLinkageProjection(
        snapshot_to_release_link_count=_first_non_none(
            release.get("evidence_query_snapshot_to_release_link_count"),
            1 if truthy_coercer(snapshot_link.get("snapshot_diff_query_present")) else 0,
        ),
        snapshot_to_release_blocker_count=_first_non_none(
            release.get("evidence_query_snapshot_to_release_blocker_count"),
            1 if count_coercer(snapshot_link.get("release_repair_path_count")) > 0 else 0,
        ),
        snapshot_to_release_group_plan_step=_first_non_none(
            release.get("evidence_query_snapshot_to_release_group_plan_step"),
            snapshot_link.get("linked_group_plan_step"),
        ),
        snapshot_to_release_current_repo_action_count=_first_non_none(
            release.get(
                "evidence_query_snapshot_to_release_current_repo_action_count"
            ),
            1
            if truthy_coercer(snapshot_link.get("current_repo_action_required"))
            else 0,
        ),
        snapshot_to_release_link_status=snapshot_link.get("link_status"),
        snapshot_to_release_blocker_reason=snapshot_link.get(
            "release_blocker_reason"
        ),
        snapshot_to_release_linked_repair_path_id=snapshot_link.get(
            "linked_repair_path_id"
        ),
        blocker_repair_card_link_count=_first_non_none(
            release.get("blocker_repair_card_link_count"),
            1 if blocker_link else 0,
        ),
        blocker_repair_card_linked_count=_first_non_none(
            release.get("blocker_repair_card_linked_count"),
            blocker_link.get("linked_root_cause_to_release_path_count"),
        ),
        blocker_repair_card_unmatched_release_path_count=_first_non_none(
            release.get("blocker_repair_card_unmatched_release_path_count"),
            blocker_link.get("unmatched_release_repair_path_count"),
        ),
        blocker_repair_card_unmatched_root_cause_count=_first_non_none(
            release.get("blocker_repair_card_unmatched_root_cause_count"),
            blocker_link.get("unmatched_root_cause_repair_card_count"),
        ),
        blocker_repair_card_current_repo_action_count=_first_non_none(
            release.get("blocker_repair_card_current_repo_action_count"),
            blocker_link.get("release_current_repo_repair_path_count"),
        ),
        blocker_repair_card_target_or_validation_action_count=_first_non_none(
            release.get(
                "blocker_repair_card_target_or_validation_action_count"
            ),
            blocker_link.get("release_target_or_validation_repair_path_count"),
        ),
        blocker_repair_card_link_status=_first_non_none(
            release.get("blocker_repair_card_link_status"),
            blocker_link.get("link_status"),
        ),
        blocker_repair_card_root_cause_repair_card_count=blocker_link.get(
            "root_cause_repair_card_count"
        ),
        blocker_repair_card_first_release_repair_path_id=blocker_link.get(
            "first_release_repair_path_id"
        ),
        blocker_repair_card_first_release_repair_path_category=blocker_link.get(
            "first_release_repair_path_category"
        ),
        blocker_repair_card_first_release_repair_source_location=blocker_link.get(
            "first_release_repair_source_location"
        ),
    )


@dataclass(frozen=True)
class ReleasePipelineBlockerDrilldownScorecardValues:
    evidence_query_blocker_drilldown_record_count: int = 0
    evidence_query_blocker_drilldown_category_count: int = 0
    evidence_query_blocker_drilldown_source_location_gap_count: int = 0
    evidence_query_blocker_drilldown_affected_claim_count: int = 0
    evidence_query_blocker_drilldown_exact_wiring_location_count: int = 0
    evidence_query_blocker_drilldown_current_repo_action_count: int = 0
    evidence_query_blocker_drilldown_current_server_action_count: int = 0
    evidence_query_blocker_drilldown_target_machine_action_count: int = 0
    evidence_query_blocker_drilldown_target_product_action_count: int = 0


@dataclass(frozen=True)
class FinalReportEvidenceQueryBlockerDrilldownProjection:
    record_count: int
    category_count: int
    source_location_gap_count: int
    affected_claim_count: int
    exact_wiring_location_count: int
    current_repo_action_count: int
    current_server_action_count: int
    target_machine_action_count: int
    target_product_action_count: int

    def to_closure_dict(self) -> Dict[str, Any]:
        return dict(vars(self))

    def count_scorecard_values(
        self,
    ) -> ReleasePipelineBlockerDrilldownScorecardValues:
        return ReleasePipelineBlockerDrilldownScorecardValues(
            evidence_query_blocker_drilldown_record_count=self.record_count,
            evidence_query_blocker_drilldown_category_count=self.category_count,
            evidence_query_blocker_drilldown_source_location_gap_count=(
                self.source_location_gap_count
            ),
            evidence_query_blocker_drilldown_affected_claim_count=(
                self.affected_claim_count
            ),
            evidence_query_blocker_drilldown_exact_wiring_location_count=(
                self.exact_wiring_location_count
            ),
            evidence_query_blocker_drilldown_current_repo_action_count=(
                self.current_repo_action_count
            ),
            evidence_query_blocker_drilldown_current_server_action_count=(
                self.current_server_action_count
            ),
            evidence_query_blocker_drilldown_target_machine_action_count=(
                self.target_machine_action_count
            ),
            evidence_query_blocker_drilldown_target_product_action_count=(
                self.target_product_action_count
            ),
        )

    def to_release_pipeline_scorecard_dict(self) -> Dict[str, Any]:
        return project_domain_fields(
            vars(self.count_scorecard_values()),
            RELEASE_PIPELINE_BLOCKER_DRILLDOWN_SCORECARD_FIELDS,
        )


def project_final_report_evidence_query_blocker_drilldown(
    release_pipeline: Optional[Mapping[str, Any]],
    drilldown_review: Optional[Mapping[str, Any]],
    ux_summary: Optional[Mapping[str, Any]],
    *,
    count_coercer: Callable[[Any], int],
) -> FinalReportEvidenceQueryBlockerDrilldownProjection:
    """Project one canonical Final Report blocker-drilldown count view."""
    pipeline = _mapping_value(release_pipeline)
    drilldown = _mapping_value(drilldown_review)
    summary = _mapping_value(ux_summary)

    def count_value(field: str) -> int:
        return count_coercer(
            _first_non_none(
                pipeline.get(f"evidence_query_blocker_drilldown_{field}"),
                drilldown.get(field),
                summary.get(f"query_ux_blocker_drilldown_{field}"),
            )
        )

    return FinalReportEvidenceQueryBlockerDrilldownProjection(
        record_count=count_value("record_count"),
        category_count=count_value("category_count"),
        source_location_gap_count=count_value("source_location_gap_count"),
        affected_claim_count=count_value("affected_claim_count"),
        exact_wiring_location_count=count_value(
            "exact_wiring_location_count"
        ),
        current_repo_action_count=count_value("current_repo_action_count"),
        current_server_action_count=count_value(
            "current_server_action_count"
        ),
        target_machine_action_count=count_value("target_machine_action_count"),
        target_product_action_count=count_value("target_product_action_count"),
    )


@dataclass(frozen=True)
class ReleasePipelineReadinessPathCountScorecardValues:
    evidence_query_readiness_path_blocking_repair_card_count: int = 0
    evidence_query_readiness_path_step_count: int = 0
    evidence_query_readiness_path_current_repo_step_count: int = 0
    evidence_query_readiness_path_target_or_validation_step_count: int = 0
    evidence_query_readiness_path_source_location_gap_count: int = 0
    evidence_query_readiness_path_catalog_flow_discrepancy_count: int = 0
    evidence_query_readiness_path_fallback_removal_blocked_count: int = 0
    evidence_query_readiness_path_field_coverage_gap_count: int = 0


@dataclass(frozen=True)
class FinalReportEvidenceQueryReadinessPathProjection:
    target: Any
    state: Any
    blocking_repair_card_count: int
    step_count: int
    current_repo_step_count: int
    target_or_validation_step_count: int
    source_location_gap_count: int
    catalog_flow_discrepancy_count: int
    fallback_removal_blocked_count: int
    field_coverage_gap_count: int
    actionability_status: Any
    primary_environment: Any
    blocker_reason: Any
    current_repo_actionable: bool
    validation_or_target_required: bool
    source_location_metadata_required: bool
    next_action_present: bool

    def to_closure_dict(self) -> Dict[str, Any]:
        return {
            "target": self.target,
            "state": self.state,
            "blocking_repair_card_count": self.blocking_repair_card_count,
            "path_step_count": self.step_count,
            "current_repo_step_count": self.current_repo_step_count,
            "target_or_validation_step_count": (
                self.target_or_validation_step_count
            ),
            "source_location_gap_count": self.source_location_gap_count,
            "catalog_flow_discrepancy_count": (
                self.catalog_flow_discrepancy_count
            ),
            "fallback_removal_blocked_count": (
                self.fallback_removal_blocked_count
            ),
            "field_coverage_gap_count": self.field_coverage_gap_count,
            "actionability_status": self.actionability_status,
            "primary_environment": self.primary_environment,
            "blocker_reason": self.blocker_reason,
            "current_repo_actionable": self.current_repo_actionable,
            "validation_or_target_required": (
                self.validation_or_target_required
            ),
            "source_location_metadata_required": (
                self.source_location_metadata_required
            ),
            "next_action_present": self.next_action_present,
        }

    def count_scorecard_values(
        self,
    ) -> ReleasePipelineReadinessPathCountScorecardValues:
        return ReleasePipelineReadinessPathCountScorecardValues(
            evidence_query_readiness_path_blocking_repair_card_count=(
                self.blocking_repair_card_count
            ),
            evidence_query_readiness_path_step_count=self.step_count,
            evidence_query_readiness_path_current_repo_step_count=(
                self.current_repo_step_count
            ),
            evidence_query_readiness_path_target_or_validation_step_count=(
                self.target_or_validation_step_count
            ),
            evidence_query_readiness_path_source_location_gap_count=(
                self.source_location_gap_count
            ),
            evidence_query_readiness_path_catalog_flow_discrepancy_count=(
                self.catalog_flow_discrepancy_count
            ),
            evidence_query_readiness_path_fallback_removal_blocked_count=(
                self.fallback_removal_blocked_count
            ),
            evidence_query_readiness_path_field_coverage_gap_count=(
                self.field_coverage_gap_count
            ),
        )

    def to_release_pipeline_scorecard_dict(
        self,
        *,
        truthy_coercer: Callable[[Any], bool],
    ) -> Dict[str, Any]:
        scorecard = project_domain_fields(
            vars(self.count_scorecard_values()),
            RELEASE_PIPELINE_READINESS_PATH_COUNT_SCORECARD_FIELDS,
        )
        boolean_values = vars(
            ReleasePipelineNativeBooleanSignalScorecardValues(
                evidence_query_readiness_path_current_repo_actionable=(
                    self.current_repo_actionable
                ),
                evidence_query_readiness_path_validation_or_target_required=(
                    self.validation_or_target_required
                ),
                evidence_query_readiness_path_source_location_metadata_required=(
                    self.source_location_metadata_required
                ),
                evidence_query_readiness_path_next_action_present=(
                    self.next_action_present
                ),
            )
        )
        for view_name in (
            "readiness_current_repo_actionable",
            "readiness_validation_or_target_required",
            "readiness_source_location_metadata_required",
            "readiness_next_action_present",
        ):
            scorecard.update(
                project_domain_fields(
                    boolean_values,
                    RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_VIEWS[
                        view_name
                    ],
                    truthy_coercer=truthy_coercer,
                )
            )
        scorecard.update(
            {
                "release_pipeline_evidence_query_readiness_path_target": (
                    self.target
                ),
                "release_pipeline_evidence_query_readiness_path_state": (
                    self.state
                ),
                "release_pipeline_evidence_query_readiness_path_actionability_status": (
                    self.actionability_status
                ),
                "release_pipeline_evidence_query_readiness_path_primary_environment": (
                    self.primary_environment
                ),
                "release_pipeline_evidence_query_readiness_path_blocker_reason": (
                    self.blocker_reason
                ),
            }
        )
        return scorecard


def project_final_report_evidence_query_readiness_path(
    release_pipeline: Optional[Mapping[str, Any]],
    drilldown_review: Optional[Mapping[str, Any]],
    ux_summary: Optional[Mapping[str, Any]],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
    preserve_missing: bool = False,
) -> FinalReportEvidenceQueryReadinessPathProjection:
    """Project one canonical Final Report Evidence Query readiness path."""
    pipeline = _mapping_value(release_pipeline)
    drilldown = _mapping_value(drilldown_review)
    summary = _mapping_value(ux_summary)
    projection_default = partial(
        missing_preserving_default,
        preserve_missing=preserve_missing,
    )

    def source_value(
        field: str,
        drilldown_field: Optional[str] = None,
        default: Any = None,
    ) -> Any:
        return _first_non_none(
            pipeline.get(f"evidence_query_readiness_path_{field}"),
            drilldown.get(drilldown_field or field),
            summary.get(f"query_ux_readiness_path_{field}"),
            default,
        )

    return FinalReportEvidenceQueryReadinessPathProjection(
        target=source_value("target", default=projection_default("unknown")),
        state=source_value("state", default=projection_default("unknown")),
        blocking_repair_card_count=count_coercer(
            source_value("blocking_repair_card_count")
        ),
        step_count=count_coercer(source_value("step_count", "path_step_count")),
        current_repo_step_count=count_coercer(
            source_value("current_repo_step_count")
        ),
        target_or_validation_step_count=count_coercer(
            source_value("target_or_validation_step_count")
        ),
        source_location_gap_count=count_coercer(
            source_value("source_location_gap_count")
        ),
        catalog_flow_discrepancy_count=count_coercer(
            source_value("catalog_flow_discrepancy_count")
        ),
        fallback_removal_blocked_count=count_coercer(
            source_value("fallback_removal_blocked_count")
        ),
        field_coverage_gap_count=count_coercer(
            source_value("field_coverage_gap_count")
        ),
        actionability_status=source_value(
            "actionability_status", default=projection_default("unknown")
        ),
        primary_environment=source_value(
            "primary_environment", default=projection_default("unknown")
        ),
        blocker_reason=source_value(
            "blocker_reason", default=projection_default("unknown")
        ),
        current_repo_actionable=truthy_coercer(
            source_value(
                "current_repo_actionable",
                default=projection_default(False),
            )
        ),
        validation_or_target_required=truthy_coercer(
            source_value(
                "validation_or_target_required",
                default=projection_default(False),
            )
        ),
        source_location_metadata_required=truthy_coercer(
            source_value(
                "source_location_metadata_required",
                default=projection_default(False),
            )
        ),
        next_action_present=truthy_coercer(
            source_value(
                "next_action_present", default=projection_default(False)
            )
        ),
    )


@dataclass(frozen=True)
class ReleasePipelineSourceExactnessCountScorecardValues:
    evidence_query_repair_card_source_exactness_card_count: int = 0
    evidence_query_repair_card_source_exactness_exact_card_count: int = 0
    evidence_query_repair_card_source_exactness_gap_card_count: int = 0
    evidence_query_repair_card_source_exactness_current_repo_gap_count: int = 0
    evidence_query_repair_card_source_exactness_target_or_validation_gap_count: int = 0
    evidence_query_repair_card_source_exactness_percent: int = 0
    evidence_query_explain_tree_node_count: int = 0
    evidence_query_explain_tree_source_exact_node_count: int = 0
    evidence_query_explain_tree_source_gap_node_count: int = 0
    evidence_query_explain_tree_current_repo_source_gap_count: int = 0
    evidence_query_explain_tree_target_or_validation_source_gap_count: int = 0
    evidence_query_explain_tree_source_exactness_percent: int = 0


@dataclass(frozen=True)
class FinalReportEvidenceQuerySourceExactnessProjection:
    repair_card_source_exactness_card_count: int
    repair_card_source_exactness_exact_card_count: int
    repair_card_source_exactness_gap_card_count: int
    repair_card_source_exactness_current_repo_gap_count: int
    repair_card_source_exactness_target_or_validation_gap_count: int
    repair_card_source_exactness_percent: int
    explain_tree_node_count: int
    explain_tree_source_exact_node_count: int
    explain_tree_source_gap_node_count: int
    explain_tree_current_repo_source_gap_count: int
    explain_tree_target_or_validation_source_gap_count: int
    explain_tree_source_exactness_percent: int

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return {
            f"evidence_query_{name}": value
            for name, value in vars(self).items()
        }

    def to_explain_tree_closure_dict(self) -> Dict[str, Any]:
        prefix = "explain_tree_"
        return {
            name[len(prefix) :]: value
            for name, value in vars(self).items()
            if name.startswith(prefix)
        }

    def count_scorecard_values(
        self,
    ) -> ReleasePipelineSourceExactnessCountScorecardValues:
        return ReleasePipelineSourceExactnessCountScorecardValues(
            **self.to_release_pipeline_dict()
        )

    def to_release_pipeline_scorecard_dict(self) -> Dict[str, Any]:
        return project_domain_fields(
            vars(self.count_scorecard_values()),
            RELEASE_PIPELINE_SOURCE_EXACTNESS_COUNT_SCORECARD_FIELDS,
        )


def project_final_report_evidence_query_source_exactness(
    release_pipeline: Optional[Mapping[str, Any]],
    repair_group_plan: Optional[Mapping[str, Any]],
    explain_tree_review: Optional[Mapping[str, Any]],
    ux_summary: Optional[Mapping[str, Any]],
    *,
    count_coercer: Callable[[Any], int],
) -> FinalReportEvidenceQuerySourceExactnessProjection:
    """Project Repair Card and Explain Tree source-exactness counts."""
    pipeline = _mapping_value(release_pipeline)
    repair_plan = _mapping_value(repair_group_plan)
    explain_review = _mapping_value(explain_tree_review)
    summary = _mapping_value(ux_summary)

    def repair_count(suffix: str) -> int:
        name = f"repair_card_source_exactness_{suffix}"
        return count_coercer(
            _first_non_none(
                pipeline.get(f"evidence_query_{name}"),
                repair_plan.get(name),
                summary.get(f"query_ux_{name}"),
            )
        )

    def explain_count(suffix: str) -> int:
        name = f"explain_tree_{suffix}"
        return count_coercer(
            _first_non_none(
                pipeline.get(f"evidence_query_{name}"),
                explain_review.get(suffix),
                summary.get(f"query_ux_{name}"),
            )
        )

    return FinalReportEvidenceQuerySourceExactnessProjection(
        repair_card_source_exactness_card_count=repair_count("card_count"),
        repair_card_source_exactness_exact_card_count=repair_count(
            "exact_card_count"
        ),
        repair_card_source_exactness_gap_card_count=repair_count(
            "gap_card_count"
        ),
        repair_card_source_exactness_current_repo_gap_count=repair_count(
            "current_repo_gap_count"
        ),
        repair_card_source_exactness_target_or_validation_gap_count=repair_count(
            "target_or_validation_gap_count"
        ),
        repair_card_source_exactness_percent=repair_count("percent"),
        explain_tree_node_count=explain_count("node_count"),
        explain_tree_source_exact_node_count=explain_count(
            "source_exact_node_count"
        ),
        explain_tree_source_gap_node_count=explain_count(
            "source_gap_node_count"
        ),
        explain_tree_current_repo_source_gap_count=explain_count(
            "current_repo_source_gap_count"
        ),
        explain_tree_target_or_validation_source_gap_count=explain_count(
            "target_or_validation_source_gap_count"
        ),
        explain_tree_source_exactness_percent=explain_count(
            "source_exactness_percent"
        ),
    )


@dataclass(frozen=True)
class ReleasePipelineRepairCardNavigationCountScorecardValues:
    evidence_query_repair_card_group_count: int = 0
    evidence_query_repair_card_group_card_count: int = 0
    evidence_query_repair_card_group_current_repo_count: int = 0
    evidence_query_repair_card_group_current_server_count: int = 0
    evidence_query_repair_card_group_target_machine_count: int = 0
    evidence_query_repair_card_group_target_product_count: int = 0
    evidence_query_repair_card_group_source_location_gap_count: int = 0
    evidence_query_repair_card_path_rank_count: int = 0
    evidence_query_repair_card_path_current_repo_rank_count: int = 0
    evidence_query_repair_card_path_target_or_validation_rank_count: int = 0
    evidence_query_repair_card_path_source_location_gap_count: int = 0
    evidence_query_repair_card_rank_readiness_link_count: int = 0
    evidence_query_repair_card_rank_readiness_current_repo_link_count: int = 0
    evidence_query_repair_card_rank_readiness_target_or_validation_link_count: int = 0
    evidence_query_repair_card_rank_readiness_report_ready_link_count: int = 0
    evidence_query_repair_card_rank_readiness_replay_ready_link_count: int = 0
    evidence_query_repair_card_rank_readiness_execute_ready_link_count: int = 0

    def to_release_pipeline_scorecard_dict(self) -> Dict[str, Any]:
        values = vars(self)
        scorecard: Dict[str, Any] = {}
        for fields in (
            RELEASE_PIPELINE_REPAIR_CARD_GROUP_HEADLINE_COUNT_SCORECARD_FIELDS,
            RELEASE_PIPELINE_REPAIR_CARD_GROUP_DETAIL_COUNT_SCORECARD_FIELDS,
            RELEASE_PIPELINE_REPAIR_CARD_PATH_READINESS_COUNT_SCORECARD_FIELDS,
        ):
            scorecard.update(project_domain_fields(values, fields))
        return scorecard


_REPAIR_NAVIGATION_GROUP_PLAN_ALIASES = {
    "evidence_query_repair_card_group_current_repo_count": (
        "current_repo_group_count"
    ),
    "evidence_query_repair_card_group_current_server_count": (
        "current_server_group_count"
    ),
    "evidence_query_repair_card_group_target_machine_count": (
        "target_machine_group_count"
    ),
    "evidence_query_repair_card_group_target_product_count": (
        "target_product_group_count"
    ),
    "evidence_query_repair_card_group_source_location_gap_count": (
        "source_location_gap_count"
    ),
}

_REPAIR_NAVIGATION_FIELD_NAMES = tuple(
    field.name
    for field in dataclass_fields(
        ReleasePipelineRepairCardNavigationCountScorecardValues
    )
)
if any(
    not field_name.startswith("evidence_query_")
    for field_name in _REPAIR_NAVIGATION_FIELD_NAMES
):
    raise ValueError("repair-navigation fields must use evidence_query_ names")
if not set(_REPAIR_NAVIGATION_GROUP_PLAN_ALIASES).issubset(
    _REPAIR_NAVIGATION_FIELD_NAMES
):
    raise ValueError("repair-navigation aliases contain unknown fields")


def project_final_report_evidence_query_repair_navigation(
    release_summary: Mapping[str, Any],
    repair_group_plan: Mapping[str, Any],
    *,
    value_coercer: Callable[[Any], Any] = lambda value: value,
) -> ReleasePipelineRepairCardNavigationCountScorecardValues:
    """Project repair groups and readiness ranks from one source-order owner."""

    values: Dict[str, Any] = {}
    for field_name in _REPAIR_NAVIGATION_FIELD_NAMES:
        group_plan_field = _REPAIR_NAVIGATION_GROUP_PLAN_ALIASES.get(
            field_name,
            field_name[len("evidence_query_") :],
        )
        values[field_name] = value_coercer(
            first_present(
                release_summary.get(field_name),
                repair_group_plan.get(group_plan_field),
            )
        )
    return ReleasePipelineRepairCardNavigationCountScorecardValues(**values)


@dataclass(frozen=True)
class FinalReportEvidenceQuerySelectorActionPlanProjection:
    decision: Any
    step_count: Any
    current_repo_step_count: Any
    target_or_validation_step_count: Any
    source_location_gap_count: Any
    readiness_blocker_count: Any
    readiness_state: Any
    payload_selector_focus_present: Any
    payload_selector_focus_status: Any
    payload_selector_focus_match_count: Any
    payload_selector_focus_ambiguous: Any
    payload_selector_focus_source_location_gap_count: Any

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return {
            "release_pipeline_evidence_query_selector_action_plan_decision": (
                self.decision
            ),
            "release_pipeline_evidence_query_selector_action_plan_step_count": (
                self.step_count
            ),
            "release_pipeline_evidence_query_selector_action_plan_current_repo_step_count": (
                self.current_repo_step_count
            ),
            "release_pipeline_evidence_query_selector_action_plan_target_or_validation_step_count": (
                self.target_or_validation_step_count
            ),
            "release_pipeline_evidence_query_selector_action_plan_source_location_gap_count": (
                self.source_location_gap_count
            ),
            "release_pipeline_evidence_query_selector_action_plan_readiness_blocker_count": (
                self.readiness_blocker_count
            ),
            "release_pipeline_evidence_query_selector_action_plan_readiness_state": (
                self.readiness_state
            ),
            "release_pipeline_evidence_query_payload_selector_focus_present": (
                self.payload_selector_focus_present
            ),
            "release_pipeline_evidence_query_payload_selector_focus_status": (
                self.payload_selector_focus_status
            ),
            "release_pipeline_evidence_query_payload_selector_focus_match_count": (
                self.payload_selector_focus_match_count
            ),
            "release_pipeline_evidence_query_payload_selector_focus_ambiguous": (
                self.payload_selector_focus_ambiguous
            ),
            "release_pipeline_evidence_query_payload_selector_focus_source_location_gap_count": (
                self.payload_selector_focus_source_location_gap_count
            ),
        }


def project_final_report_evidence_query_selector_action_plan(
    release_summary: Mapping[str, Any],
    selector_resolution: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any] = lambda value: value,
) -> FinalReportEvidenceQuerySelectorActionPlanProjection:
    """Project selector action-plan guidance without executing repair actions."""

    def select(field_name: str, *, default: Any = None) -> Any:
        selector_field = field_name[len("evidence_query_") :]
        return first_present(
            release_summary.get(field_name),
            selector_resolution.get(selector_field),
            default,
        )

    return FinalReportEvidenceQuerySelectorActionPlanProjection(
        decision=select(
            "evidence_query_selector_action_plan_decision",
            default="unknown",
        ),
        step_count=count_coercer(
            select("evidence_query_selector_action_plan_step_count")
        ),
        current_repo_step_count=count_coercer(
            select(
                "evidence_query_selector_action_plan_current_repo_step_count"
            )
        ),
        target_or_validation_step_count=count_coercer(
            select(
                "evidence_query_selector_action_plan_target_or_validation_step_count"
            )
        ),
        source_location_gap_count=count_coercer(
            select(
                "evidence_query_selector_action_plan_source_location_gap_count"
            )
        ),
        readiness_blocker_count=count_coercer(
            select(
                "evidence_query_selector_action_plan_readiness_blocker_count"
            )
        ),
        readiness_state=select(
            "evidence_query_selector_action_plan_readiness_state",
            default="unknown",
        ),
        payload_selector_focus_present=count_coercer(
            release_summary.get("evidence_query_payload_selector_focus_present")
        ),
        payload_selector_focus_status=first_present(
            release_summary.get("evidence_query_payload_selector_focus_status"),
            "unknown",
        ),
        payload_selector_focus_match_count=count_coercer(
            release_summary.get("evidence_query_payload_selector_focus_match_count")
        ),
        payload_selector_focus_ambiguous=count_coercer(
            release_summary.get("evidence_query_payload_selector_focus_ambiguous")
        ),
        payload_selector_focus_source_location_gap_count=count_coercer(
            release_summary.get(
                "evidence_query_payload_selector_focus_source_location_gap_count"
            )
        ),
    )


@dataclass(frozen=True)
class FinalReportReleasePipelineHeadlineProjection:
    release_decision: Any
    executed_input_count: int
    execute_verified_input_count: int

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return {
            "release_decision": self.release_decision,
            "release_pipeline_executed_input_count": self.executed_input_count,
            "release_pipeline_execute_verified_input_count": (
                self.execute_verified_input_count
            ),
        }


def project_final_report_release_pipeline_headline(
    release_pipeline: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
) -> FinalReportReleasePipelineHeadlineProjection:
    """Project direct Pipeline headline fields without recomputing readiness."""
    release = _mapping_value(release_pipeline)
    return FinalReportReleasePipelineHeadlineProjection(
        release_decision=release.get("release_decision", "unknown"),
        executed_input_count=count_coercer(release.get("executed_input_count")),
        execute_verified_input_count=count_coercer(
            release.get("execute_verified_input_count")
        ),
    )


@dataclass(frozen=True)
class ReleasePipelineSourceLocationCountScorecardValues:
    evidence_query_source_location_worklist_item_count: int = 0
    evidence_query_source_location_worklist_current_repo_item_count: int = 0
    evidence_query_source_location_worklist_target_or_validation_item_count: int = 0
    evidence_query_source_location_worklist_gap_count: int = 0
    evidence_query_source_location_category_count: int = 0
    evidence_query_source_location_active_category_count: int = 0
    evidence_query_source_location_exact_category_count: int = 0
    evidence_query_source_location_gap_category_count: int = 0
    evidence_query_source_location_current_repo_gap_category_count: int = 0
    evidence_query_source_location_target_or_validation_gap_category_count: int = 0
    evidence_query_source_location_gap_repair_card_count: int = 0
    evidence_query_source_location_category_exactness_percent: int = 0


@dataclass(frozen=True)
class ReleasePipelineRepairGuidanceCountScorecardValues:
    evidence_query_repair_group_exact_wiring_location_count: int = 0
    evidence_query_repair_group_affected_claim_count: int = 0
    evidence_query_repair_narrative_step_count: int = 0
    evidence_query_repair_narrative_current_repo_action_count: int = 0
    evidence_query_repair_narrative_target_or_validation_action_count: int = 0
    evidence_query_repair_narrative_source_location_gap_count: int = 0
    evidence_query_repair_narrative_readiness_blocker_count: int = 0
    evidence_query_repair_narrative_affected_claim_count: int = 0
    evidence_query_repair_narrative_exact_wiring_location_count: int = 0
    evidence_query_repair_group_navigation_step_count: int = 0
    evidence_query_repair_group_navigation_current_repo_step_count: int = 0
    evidence_query_repair_group_navigation_target_or_validation_step_count: int = 0


@dataclass(frozen=True)
class FinalReportEvidenceQueryRepairGuidanceProjection:
    count_values: ReleasePipelineRepairGuidanceCountScorecardValues
    repair_group_summary_present: Any
    repair_narrative_present: Any
    repair_narrative_decision: Any
    repair_narrative_readiness_state: Any
    repair_group_primary_action_present: Any
    repair_group_primary_source_location_present: Any
    repair_group_primary_environment: Any
    repair_group_primary_category: Any
    repair_group_primary_artifact: Any

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return {
            **vars(self.count_values),
            "evidence_query_repair_group_summary_present": (
                self.repair_group_summary_present
            ),
            "evidence_query_repair_narrative_present": (
                self.repair_narrative_present
            ),
            "evidence_query_repair_narrative_decision": (
                self.repair_narrative_decision
            ),
            "evidence_query_repair_narrative_readiness_state": (
                self.repair_narrative_readiness_state
            ),
            "evidence_query_repair_group_primary_action_present": (
                self.repair_group_primary_action_present
            ),
            "evidence_query_repair_group_primary_source_location_present": (
                self.repair_group_primary_source_location_present
            ),
            "evidence_query_repair_group_primary_environment": (
                self.repair_group_primary_environment
            ),
            "evidence_query_repair_group_primary_category": (
                self.repair_group_primary_category
            ),
            "evidence_query_repair_group_primary_artifact": (
                self.repair_group_primary_artifact
            ),
        }

    def to_release_pipeline_scorecard_dict(self) -> Dict[str, Any]:
        scorecard: Dict[str, Any] = {}
        count_values = vars(self.count_values)
        for fields in (
            RELEASE_PIPELINE_REPAIR_GUIDANCE_GROUP_COUNT_SCORECARD_FIELDS,
            RELEASE_PIPELINE_REPAIR_GUIDANCE_NARRATIVE_COUNT_SCORECARD_FIELDS,
            RELEASE_PIPELINE_REPAIR_GUIDANCE_NAVIGATION_COUNT_SCORECARD_FIELDS,
        ):
            scorecard.update(project_domain_fields(count_values, fields))

        boolean_values = vars(
            ReleasePipelineNumericBooleanSignalScorecardValues(
                evidence_query_repair_group_summary_present=(
                    self.repair_group_summary_present
                ),
                evidence_query_repair_narrative_present=(
                    self.repair_narrative_present
                ),
                evidence_query_repair_group_primary_action_present=(
                    self.repair_group_primary_action_present
                ),
                evidence_query_repair_group_primary_source_location_present=(
                    self.repair_group_primary_source_location_present
                ),
            )
        )
        for view_name in (
            "repair_group_summary_present",
            "repair_narrative_present",
            "repair_group_primary_action_present",
            "repair_group_primary_source_location_present",
        ):
            scorecard.update(
                project_domain_fields(
                    boolean_values,
                    RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_VIEWS[
                        view_name
                    ],
                )
            )
        scorecard.update(
            {
                "release_pipeline_evidence_query_repair_narrative_decision": (
                    self.repair_narrative_decision
                ),
                "release_pipeline_evidence_query_repair_narrative_readiness_state": (
                    self.repair_narrative_readiness_state
                ),
                "release_pipeline_evidence_query_repair_group_primary_environment": (
                    self.repair_group_primary_environment
                ),
                "release_pipeline_evidence_query_repair_group_primary_category": (
                    self.repair_group_primary_category
                ),
                "release_pipeline_evidence_query_repair_group_primary_artifact": (
                    self.repair_group_primary_artifact
                ),
            }
        )
        return scorecard


_REPAIR_GUIDANCE_PLAN_ALIASES = {
    "evidence_query_repair_group_exact_wiring_location_count": (
        "exact_wiring_location_count"
    ),
    "evidence_query_repair_group_affected_claim_count": "affected_claim_count",
    "evidence_query_repair_group_summary_present": "repair_group_summary_present",
    "evidence_query_repair_group_primary_action_present": "primary_action_present",
    "evidence_query_repair_group_primary_source_location_present": (
        "primary_source_location_present"
    ),
    "evidence_query_repair_group_primary_environment": (
        "primary_repair_environment"
    ),
    "evidence_query_repair_group_primary_category": "primary_category",
    "evidence_query_repair_group_primary_artifact": "primary_artifact",
}
_REPAIR_GUIDANCE_COUNT_FIELD_NAMES = tuple(
    field.name
    for field in dataclass_fields(
        ReleasePipelineRepairGuidanceCountScorecardValues
    )
)
_REPAIR_GUIDANCE_NON_COUNT_FIELD_NAMES = (
    "evidence_query_repair_group_summary_present",
    "evidence_query_repair_narrative_present",
    "evidence_query_repair_narrative_decision",
    "evidence_query_repair_narrative_readiness_state",
    "evidence_query_repair_group_primary_action_present",
    "evidence_query_repair_group_primary_source_location_present",
    "evidence_query_repair_group_primary_environment",
    "evidence_query_repair_group_primary_category",
    "evidence_query_repair_group_primary_artifact",
)
if set(_REPAIR_GUIDANCE_PLAN_ALIASES) - set(
    _REPAIR_GUIDANCE_COUNT_FIELD_NAMES
    + _REPAIR_GUIDANCE_NON_COUNT_FIELD_NAMES
):
    raise ValueError("repair-guidance aliases contain unknown fields")


def project_final_report_evidence_query_repair_guidance(
    release_summary: Mapping[str, Any],
    repair_group_plan: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any] = lambda value: value,
    presence_coercer: Callable[[Any], Any] = lambda value: value,
    preserve_missing: bool = False,
) -> FinalReportEvidenceQueryRepairGuidanceProjection:
    """Project repair narrative and navigation from one source-order owner."""

    def select(field_name: str, *, default: Any = None) -> Any:
        plan_field = _REPAIR_GUIDANCE_PLAN_ALIASES.get(
            field_name,
            field_name[len("evidence_query_") :],
        )
        return first_present(
            release_summary.get(field_name),
            repair_group_plan.get(plan_field),
            default,
        )

    count_values = ReleasePipelineRepairGuidanceCountScorecardValues(
        **{
            field_name: count_coercer(select(field_name))
            for field_name in _REPAIR_GUIDANCE_COUNT_FIELD_NAMES
        }
    )
    text_default = missing_preserving_default(
        "unknown", preserve_missing=preserve_missing
    )
    return FinalReportEvidenceQueryRepairGuidanceProjection(
        count_values=count_values,
        repair_group_summary_present=presence_coercer(
            select("evidence_query_repair_group_summary_present")
        ),
        repair_narrative_present=presence_coercer(
            select("evidence_query_repair_narrative_present")
        ),
        repair_narrative_decision=select(
            "evidence_query_repair_narrative_decision",
            default=text_default,
        ),
        repair_narrative_readiness_state=select(
            "evidence_query_repair_narrative_readiness_state",
            default=text_default,
        ),
        repair_group_primary_action_present=presence_coercer(
            select("evidence_query_repair_group_primary_action_present")
        ),
        repair_group_primary_source_location_present=presence_coercer(
            select(
                "evidence_query_repair_group_primary_source_location_present"
            )
        ),
        repair_group_primary_environment=select(
            "evidence_query_repair_group_primary_environment",
            default=text_default,
        ),
        repair_group_primary_category=select(
            "evidence_query_repair_group_primary_category",
            default=text_default,
        ),
        repair_group_primary_artifact=select(
            "evidence_query_repair_group_primary_artifact",
            default=text_default,
        ),
    )


@dataclass(frozen=True)
class FinalReportEvidenceQuerySourceLocationProjection:
    count_values: ReleasePipelineSourceLocationCountScorecardValues
    worklist_focus_present: Any
    worklist_focus_first_item_present: Any
    worklist_focus_first_gap_present: Any
    worklist_focus_first_item_current_repo_action_required: Any
    worklist_focus_first_item_target_or_validation_action_required: Any

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return {
            **vars(self.count_values),
            "evidence_query_source_location_worklist_focus_present": (
                self.worklist_focus_present
            ),
            "evidence_query_source_location_worklist_focus_first_item_present": (
                self.worklist_focus_first_item_present
            ),
            "evidence_query_source_location_worklist_focus_first_gap_present": (
                self.worklist_focus_first_gap_present
            ),
            "evidence_query_source_location_worklist_focus_first_item_current_repo_action_required": (
                self.worklist_focus_first_item_current_repo_action_required
            ),
            "evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required": (
                self.worklist_focus_first_item_target_or_validation_action_required
            ),
        }

    def to_release_pipeline_scorecard_dict(self) -> Dict[str, Any]:
        scorecard: Dict[str, Any] = {}
        count_values = vars(self.count_values)
        for fields in (
            RELEASE_PIPELINE_SOURCE_LOCATION_WORKLIST_COUNT_SCORECARD_FIELDS,
            RELEASE_PIPELINE_SOURCE_LOCATION_CATEGORY_COUNT_SCORECARD_FIELDS,
        ):
            scorecard.update(project_domain_fields(count_values, fields))

        boolean_values = vars(
            ReleasePipelineNumericBooleanSignalScorecardValues(
                evidence_query_source_location_worklist_focus_present=(
                    self.worklist_focus_present
                ),
                evidence_query_source_location_worklist_focus_first_item_present=(
                    self.worklist_focus_first_item_present
                ),
                evidence_query_source_location_worklist_focus_first_gap_present=(
                    self.worklist_focus_first_gap_present
                ),
                evidence_query_source_location_worklist_focus_first_item_current_repo_action_required=(
                    self.worklist_focus_first_item_current_repo_action_required
                ),
                evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required=(
                    self.worklist_focus_first_item_target_or_validation_action_required
                ),
            )
        )
        scorecard.update(
            project_domain_fields(
                boolean_values,
                RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_VIEWS[
                    "source_location_worklist_focus"
                ],
            )
        )
        return scorecard


_SOURCE_LOCATION_CATEGORY_COVERAGE_ALIASES = {
    "evidence_query_source_location_category_count": "category_count",
    "evidence_query_source_location_active_category_count": (
        "active_category_count"
    ),
    "evidence_query_source_location_exact_category_count": (
        "source_exact_category_count"
    ),
    "evidence_query_source_location_gap_category_count": (
        "source_gap_category_count"
    ),
    "evidence_query_source_location_current_repo_gap_category_count": (
        "current_repo_source_gap_category_count"
    ),
    "evidence_query_source_location_target_or_validation_gap_category_count": (
        "target_or_validation_source_gap_category_count"
    ),
    "evidence_query_source_location_gap_repair_card_count": (
        "source_gap_repair_card_count"
    ),
    "evidence_query_source_location_category_exactness_percent": (
        "source_exactness_percent"
    ),
}
_SOURCE_LOCATION_COUNT_FIELD_NAMES = tuple(
    field.name
    for field in dataclass_fields(
        ReleasePipelineSourceLocationCountScorecardValues
    )
)
if set(_SOURCE_LOCATION_CATEGORY_COVERAGE_ALIASES) - set(
    _SOURCE_LOCATION_COUNT_FIELD_NAMES
):
    raise ValueError("source-location coverage aliases contain unknown fields")


def project_final_report_evidence_query_source_location(
    release_summary: Mapping[str, Any],
    repair_group_plan: Mapping[str, Any],
    category_coverage: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any] = lambda value: value,
) -> FinalReportEvidenceQuerySourceLocationProjection:
    """Project source-location worklist and coverage from canonical sources."""

    def select(field_name: str) -> Any:
        coverage_field = _SOURCE_LOCATION_CATEGORY_COVERAGE_ALIASES.get(
            field_name
        )
        plan_field = field_name[len("evidence_query_") :]
        return first_present(
            release_summary.get(field_name),
            category_coverage.get(coverage_field) if coverage_field else None,
            repair_group_plan.get(plan_field),
        )

    count_values = ReleasePipelineSourceLocationCountScorecardValues(
        **{
            field_name: count_coercer(select(field_name))
            for field_name in _SOURCE_LOCATION_COUNT_FIELD_NAMES
        }
    )
    return FinalReportEvidenceQuerySourceLocationProjection(
        count_values=count_values,
        worklist_focus_present=count_coercer(
            select("evidence_query_source_location_worklist_focus_present")
        ),
        worklist_focus_first_item_present=count_coercer(
            select(
                "evidence_query_source_location_worklist_focus_first_item_present"
            )
        ),
        worklist_focus_first_gap_present=count_coercer(
            select(
                "evidence_query_source_location_worklist_focus_first_gap_present"
            )
        ),
        worklist_focus_first_item_current_repo_action_required=count_coercer(
            select(
                "evidence_query_source_location_worklist_focus_first_item_current_repo_action_required"
            )
        ),
        worklist_focus_first_item_target_or_validation_action_required=count_coercer(
            select(
                "evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required"
            )
        ),
    )


@dataclass(frozen=True)
class ReleasePipelineCatalogFallbackRemovalCountScorecardValues:
    evidence_query_catalog_flow_fallback_removal_shortest_path_step_count: int = 0
    evidence_query_catalog_flow_fallback_removal_shortest_path_current_repo_step_count: int = 0
    evidence_query_catalog_flow_fallback_removal_shortest_path_target_machine_step_count: int = 0
    evidence_query_catalog_flow_fallback_removal_shortest_path_source_location_gap_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_blocker_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_blocking_surface_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_current_repo_action_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_target_machine_action_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_source_location_gap_count: int = 0
    evidence_query_catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count: int = 0
    root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count: int = 0
    root_cause_catalog_flow_fallback_removal_shortest_path_step_count: int = 0
    root_cause_catalog_flow_fallback_removal_shortest_path_current_repo_step_count: int = 0
    root_cause_catalog_flow_fallback_removal_shortest_path_target_machine_step_count: int = 0
    root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_blocking_surface_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_current_repo_action_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_target_machine_action_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count: int = 0
    root_cause_catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count: int = 0


@dataclass(frozen=True)
class ReleasePipelineSelectorReplayCountScorecardValues:
    evidence_query_payload_selector_focus_match_count: int = 0
    evidence_query_payload_selector_focus_source_location_gap_count: int = 0
    evidence_query_product_replay_semantic_linkage_blocker_count: int = 0
    evidence_query_product_replay_semantic_linkage_pending_event_count: int = 0
    evidence_query_product_replay_semantic_linkage_correlation_count: int = 0
    evidence_query_product_replay_semantic_linkage_target_or_validation_action_count: int = 0


@dataclass(frozen=True)
class ReleasePipelineNativeBooleanSignalScorecardValues:
    evidence_query_catalog_flow_fallback_removal_parity_gate_blocks_removal: bool = False
    root_cause_catalog_flow_fallback_removal_parity_gate_blocks_removal: bool = False
    evidence_query_readiness_path_current_repo_actionable: bool = False
    evidence_query_readiness_path_validation_or_target_required: bool = False
    evidence_query_readiness_path_source_location_metadata_required: bool = False
    evidence_query_readiness_path_next_action_present: bool = False
    evidence_query_cross_artifact_release_focus_current_repo_actionable: bool = False
    evidence_query_cross_artifact_release_focus_target_or_validation_required: bool = False
    evidence_query_cross_artifact_release_focus_source_location_metadata_required: bool = False
    evidence_query_cross_artifact_product_replay_family_present: bool = False
    evidence_query_product_replay_semantic_linkage_sample_replay_present: bool = False


@dataclass(frozen=True)
class ReleasePipelineNumericBooleanSignalScorecardValues:
    evidence_query_payload_selector_focus_present: int = 0
    evidence_query_payload_selector_focus_ambiguous: int = 0
    evidence_query_repair_group_summary_present: int = 0
    evidence_query_repair_narrative_present: int = 0
    evidence_query_repair_group_primary_action_present: int = 0
    evidence_query_repair_group_primary_source_location_present: int = 0
    evidence_query_source_location_worklist_focus_present: int = 0
    evidence_query_source_location_worklist_focus_first_item_present: int = 0
    evidence_query_source_location_worklist_focus_first_gap_present: int = 0
    evidence_query_source_location_worklist_focus_first_item_current_repo_action_required: int = 0
    evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required: int = 0


@dataclass(frozen=True)
class ReleasePipelineCrossArtifactCountScorecardValues:
    evidence_query_cross_artifact_family_count: int = 0
    evidence_query_cross_artifact_active_family_count: int = 0
    evidence_query_cross_artifact_total_blocker_count: int = 0
    evidence_query_cross_artifact_current_repo_action_count: int = 0
    evidence_query_cross_artifact_target_or_validation_action_count: int = 0
    evidence_query_cross_artifact_source_location_gap_count: int = 0
    evidence_query_cross_artifact_release_focus_affected_claim_count: int = 0
    evidence_query_cross_artifact_product_replay_family_blocker_count: int = 0
    evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count: int = 0
    evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count: int = 0
    evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count: int = 0
    evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count: int = 0
    evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count: int = 0
    evidence_query_cross_artifact_product_replay_family_source_artifact_count: int = 0


@dataclass(frozen=True)
class FinalReportEvidenceQueryCrossArtifactProjection:
    status: Any
    family_count: int
    active_family_count: int
    total_blocker_count: int
    current_repo_action_count: int
    target_or_validation_action_count: int
    source_location_gap_count: int
    primary_family: Any
    primary_blocker_reason: Any
    primary_environment: Any
    release_focus_decision: Any
    release_focus_repair_lane: Any
    release_focus_affected_claim_count: int
    release_focus_repair_path_count: int
    release_focus_first_repair_family: Any
    release_focus_first_repair_environment: Any
    release_focus_first_repair_action: Any
    release_focus_primary_action: Any
    release_focus_current_repo_actionable: bool
    release_focus_target_or_validation_required: bool
    release_focus_source_location_metadata_required: bool

    def to_closure_dict(self) -> Dict[str, Any]:
        return dict(vars(self))

    def to_release_pipeline_scorecard_dict(
        self,
        *,
        truthy_coercer: Callable[[Any], bool],
    ) -> Dict[str, Any]:
        count_values = vars(
            ReleasePipelineCrossArtifactCountScorecardValues(
                evidence_query_cross_artifact_family_count=self.family_count,
                evidence_query_cross_artifact_active_family_count=(
                    self.active_family_count
                ),
                evidence_query_cross_artifact_total_blocker_count=(
                    self.total_blocker_count
                ),
                evidence_query_cross_artifact_current_repo_action_count=(
                    self.current_repo_action_count
                ),
                evidence_query_cross_artifact_target_or_validation_action_count=(
                    self.target_or_validation_action_count
                ),
                evidence_query_cross_artifact_source_location_gap_count=(
                    self.source_location_gap_count
                ),
                evidence_query_cross_artifact_release_focus_affected_claim_count=(
                    self.release_focus_affected_claim_count
                ),
            )
        )
        scorecard: Dict[str, Any] = {}
        for fields in (
            RELEASE_PIPELINE_CROSS_ARTIFACT_CORE_COUNT_SCORECARD_FIELDS,
            RELEASE_PIPELINE_CROSS_ARTIFACT_RELEASE_FOCUS_COUNT_SCORECARD_FIELDS,
        ):
            scorecard.update(project_domain_fields(count_values, fields))

        boolean_values = vars(
            ReleasePipelineNativeBooleanSignalScorecardValues(
                evidence_query_cross_artifact_release_focus_current_repo_actionable=(
                    self.release_focus_current_repo_actionable
                ),
                evidence_query_cross_artifact_release_focus_target_or_validation_required=(
                    self.release_focus_target_or_validation_required
                ),
                evidence_query_cross_artifact_release_focus_source_location_metadata_required=(
                    self.release_focus_source_location_metadata_required
                ),
            )
        )
        for view_name in (
            "cross_release_focus_current_repo_actionable",
            "cross_release_focus_target_or_validation_required",
            "cross_release_focus_source_location_metadata_required",
        ):
            scorecard.update(
                project_domain_fields(
                    boolean_values,
                    RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_VIEWS[
                        view_name
                    ],
                    truthy_coercer=truthy_coercer,
                )
            )
        scorecard.update(
            {
                "release_pipeline_evidence_query_cross_artifact_blocker_explanation_status": (
                    self.status
                ),
                "release_pipeline_evidence_query_cross_artifact_primary_family": (
                    self.primary_family
                ),
                "release_pipeline_evidence_query_cross_artifact_primary_blocker_reason": (
                    self.primary_blocker_reason
                ),
                "release_pipeline_evidence_query_cross_artifact_primary_environment": (
                    self.primary_environment
                ),
                "release_pipeline_evidence_query_cross_artifact_release_focus_decision": (
                    self.release_focus_decision
                ),
                "release_pipeline_evidence_query_cross_artifact_release_focus_repair_lane": (
                    self.release_focus_repair_lane
                ),
                "release_pipeline_evidence_query_cross_artifact_release_focus_first_repair_family": (
                    self.release_focus_first_repair_family
                ),
                "release_pipeline_evidence_query_cross_artifact_release_focus_first_repair_environment": (
                    self.release_focus_first_repair_environment
                ),
                "release_pipeline_evidence_query_cross_artifact_release_focus_first_repair_action": (
                    self.release_focus_first_repair_action
                ),
                "release_pipeline_evidence_query_cross_artifact_release_focus_primary_action": (
                    self.release_focus_primary_action
                ),
            }
        )
        return scorecard


def project_final_report_evidence_query_cross_artifact(
    release_pipeline: Optional[Mapping[str, Any]],
    cross_artifact_review: Optional[Mapping[str, Any]],
    ux_summary: Optional[Mapping[str, Any]],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
    preserve_missing: bool = False,
) -> FinalReportEvidenceQueryCrossArtifactProjection:
    """Project the canonical Final Report cross-artifact explanation view."""
    pipeline = _mapping_value(release_pipeline)
    review = _mapping_value(cross_artifact_review)
    summary = _mapping_value(ux_summary)
    projection_default = partial(
        missing_preserving_default,
        preserve_missing=preserve_missing,
    )

    def source_value(
        field: str,
        *,
        pipeline_field: Optional[str] = None,
        review_field: Optional[str] = None,
        summary_field: Optional[str] = None,
        default: Any = None,
    ) -> Any:
        return _first_non_none(
            pipeline.get(
                pipeline_field or f"evidence_query_cross_artifact_{field}"
            ),
            review.get(review_field or field),
            summary.get(summary_field or f"cross_artifact_{field}"),
            default,
        )

    return FinalReportEvidenceQueryCrossArtifactProjection(
        status=source_value(
            "status",
            pipeline_field=(
                "evidence_query_cross_artifact_blocker_explanation_status"
            ),
            summary_field="cross_artifact_blocker_explanation_status",
            default=projection_default("unknown"),
        ),
        family_count=count_coercer(
            source_value(
                "family_count",
                summary_field="cross_artifact_blocker_family_count",
            )
        ),
        active_family_count=count_coercer(
            source_value("active_family_count")
        ),
        total_blocker_count=count_coercer(
            source_value("total_blocker_count")
        ),
        current_repo_action_count=count_coercer(
            source_value("current_repo_action_count")
        ),
        target_or_validation_action_count=count_coercer(
            source_value("target_or_validation_action_count")
        ),
        source_location_gap_count=count_coercer(
            source_value("source_location_gap_count")
        ),
        primary_family=source_value(
            "primary_family", default=projection_default("unknown")
        ),
        primary_blocker_reason=source_value(
            "primary_blocker_reason", default=projection_default("unknown")
        ),
        primary_environment=source_value(
            "primary_environment", default=projection_default("unknown")
        ),
        release_focus_decision=source_value(
            "release_focus_decision", default=projection_default("unknown")
        ),
        release_focus_repair_lane=source_value(
            "release_focus_repair_lane", default=projection_default("unknown")
        ),
        release_focus_affected_claim_count=count_coercer(
            source_value("release_focus_affected_claim_count")
        ),
        release_focus_repair_path_count=count_coercer(
            source_value("release_focus_repair_path_count")
        ),
        release_focus_first_repair_family=source_value(
            "release_focus_first_repair_family",
            default=projection_default("unknown"),
        ),
        release_focus_first_repair_environment=source_value(
            "release_focus_first_repair_environment",
            default=projection_default("unknown"),
        ),
        release_focus_first_repair_action=source_value(
            "release_focus_first_repair_action",
            default=projection_default("unknown"),
        ),
        release_focus_primary_action=source_value(
            "release_focus_primary_action",
            default=projection_default("unknown"),
        ),
        release_focus_current_repo_actionable=truthy_coercer(
            source_value(
                "release_focus_current_repo_actionable",
                default=projection_default(False),
            )
        ),
        release_focus_target_or_validation_required=truthy_coercer(
            source_value(
                "release_focus_target_or_validation_required",
                default=projection_default(False),
            )
        ),
        release_focus_source_location_metadata_required=truthy_coercer(
            source_value(
                "release_focus_source_location_metadata_required",
                default=projection_default(False),
            )
        ),
    )


def _release_pipeline_count_fields(
    source_names: Sequence[str],
) -> Tuple[DomainFieldSpec, ...]:
    return tuple(
        DomainFieldSpec(
            f"release_pipeline_{source_name}",
            DOMAIN_VALUE_COUNT,
            0,
            source_name,
        )
        for source_name in source_names
    )


def _release_pipeline_truthy_fields(
    source_names: Sequence[str],
) -> Tuple[DomainFieldSpec, ...]:
    return tuple(
        DomainFieldSpec(
            f"release_pipeline_{source_name}",
            DOMAIN_VALUE_TRUTHY,
            False,
            source_name,
        )
        for source_name in source_names
    )


def _release_pipeline_numeric_boolean_fields(
    source_names: Sequence[str],
) -> Tuple[DomainFieldSpec, ...]:
    return tuple(
        DomainFieldSpec(
            f"release_pipeline_{source_name}",
            DOMAIN_VALUE_NUMERIC_BOOL,
            0,
            source_name,
        )
        for source_name in source_names
    )


def _domain_fields_by_source_names(
    fields: Sequence[DomainFieldSpec], source_names: Sequence[str]
) -> Tuple[DomainFieldSpec, ...]:
    selected_names = set(source_names)
    return tuple(
        field
        for field in fields
        if (field.source_name or field.name) in selected_names
    )


CATALOG_FLOW_CONSUMER_FIELD_COVERAGE_FIELDS = tuple(
    DomainFieldSpec(name, DOMAIN_VALUE_COUNT, 0)
    for name in (
        "catalog_flow_consumer_field_coverage_surface_count",
        "catalog_flow_consumer_field_coverage_catalog_managed_surface_count",
        "catalog_flow_consumer_field_coverage_fallback_surface_count",
        "catalog_flow_consumer_field_coverage_required_field_count",
        "catalog_flow_consumer_field_coverage_covered_field_count",
        "catalog_flow_consumer_field_coverage_field_gap_count",
        "catalog_flow_consumer_field_coverage_parity_refresh_required_count",
        "catalog_flow_consumer_field_coverage_current_repo_action_count",
        "catalog_flow_consumer_field_coverage_target_machine_action_count",
        "catalog_flow_consumer_field_coverage_repair_card_count",
        "catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
        "catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
    )
)


CATALOG_FLOW_GOVERNANCE_COUNT_FIELDS = tuple(
    DomainFieldSpec(name, DOMAIN_VALUE_COUNT, 0)
    for name in (
        "catalog_flow_input_gap_count",
        "catalog_flow_missing_surface_count",
        "catalog_flow_fallback_retained_surface_count",
        "catalog_flow_single_source_repair_card_count",
        "catalog_flow_single_source_parity_refresh_required_count",
        "catalog_flow_surface_migration_ready_count",
        "catalog_flow_surface_migration_fallback_count",
        "catalog_flow_surface_parity_refresh_required_count",
        "catalog_flow_surface_current_repo_action_count",
        "catalog_flow_surface_target_machine_action_count",
        "catalog_flow_surface_consumer_migration_plan_count",
        "catalog_flow_surface_consumer_fallback_removal_blocked_count",
        "catalog_flow_surface_consumer_parity_refresh_required_count",
        "catalog_flow_surface_consumer_current_repo_action_count",
        "catalog_flow_surface_consumer_target_machine_action_count",
        "catalog_flow_fallback_removal_queue_item_count",
        "catalog_flow_fallback_removal_queue_blocked_count",
        "catalog_flow_fallback_removal_queue_parity_refresh_required_count",
        "catalog_flow_fallback_removal_queue_current_repo_action_count",
        "catalog_flow_fallback_removal_queue_target_machine_action_count",
        "catalog_flow_fallback_removal_shortest_path_step_count",
        "catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
        "catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
        "catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
        "catalog_flow_fallback_removal_parity_gate_blocker_count",
        "catalog_flow_fallback_removal_parity_gate_blocking_surface_count",
        "catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
        "catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
        "catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
        "catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
        "catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
        "catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
        "catalog_flow_metadata_policy_card_count",
        "catalog_flow_metadata_derived_policy_artifact_count",
        "catalog_flow_metadata_derived_policy_field_count",
        "catalog_flow_missing_step_count",
        "catalog_flow_repair_card_count",
        "catalog_flow_discrepancy_count",
    )
)


CATALOG_FLOW_GOVERNANCE_TRUTHY_FIELDS = tuple(
    DomainFieldSpec(name, DOMAIN_VALUE_TRUTHY, False)
    for name in (
        "catalog_flow_discrepancy_review_present",
        "catalog_flow_fallback_removal_shortest_path_present",
        "catalog_flow_fallback_removal_shortest_path_blocks_removal",
        "catalog_flow_fallback_removal_parity_gate_present",
        "catalog_flow_fallback_removal_parity_gate_removal_allowed",
        "catalog_flow_fallback_removal_parity_gate_blocks_removal",
    )
)


CATALOG_FLOW_GOVERNANCE_TEXT_FIELDS = tuple(
    DomainFieldSpec(name, default="unknown")
    for name in (
        "catalog_flow_fallback_removal_shortest_path_state",
        "catalog_flow_fallback_removal_shortest_path_primary_environment",
        "catalog_flow_fallback_removal_shortest_path_first_surface",
        "catalog_flow_fallback_removal_parity_gate_decision",
        "catalog_flow_fallback_removal_parity_gate_first_surface",
    )
)


CATALOG_FLOW_GOVERNANCE_SUMMARY_FIELDS = (
    CATALOG_FLOW_GOVERNANCE_TRUTHY_FIELDS
    + CATALOG_FLOW_GOVERNANCE_COUNT_FIELDS
    + CATALOG_FLOW_GOVERNANCE_TEXT_FIELDS
    + CATALOG_FLOW_CONSUMER_FIELD_COVERAGE_FIELDS
)


CATALOG_FLOW_FILTER_GOVERNANCE_FIELDS = tuple(
    field
    for field in CATALOG_FLOW_GOVERNANCE_SUMMARY_FIELDS
    if not field.name.startswith(
        "catalog_flow_fallback_removal_parity_gate_"
    )
)


CATALOG_FLOW_RELEASE_SCORECARD_FIELDS = tuple(
    field
    for field in CATALOG_FLOW_GOVERNANCE_SUMMARY_FIELDS
    if field.name.startswith(
        (
            "catalog_flow_consumer_field_coverage_",
            "catalog_flow_fallback_removal_shortest_path_",
            "catalog_flow_fallback_removal_parity_gate_",
        )
    )
)


def project_evidence_query_catalog_flow_filter(
    filter_view: Mapping[str, Any],
    summary: Mapping[str, Any],
    *,
    direct_artifact_present: bool,
) -> Dict[str, Any]:
    """Project the catalog-flow query filter without a renderer-owned field table."""
    primary = _mapping_value(filter_view)
    fallback = _mapping_value(summary)
    projection: Dict[str, Any] = {
        "source": "native_evidence_query_result.json",
        "direct_artifact_present": direct_artifact_present,
        "filter_query_present": _first_non_none(
            fallback.get("catalog_flow_filter_query_present"),
            bool(primary),
        ),
        "source_artifact": primary.get(
            "source_artifact", "native_artifact_catalog_single_source.json"
        ),
        "catalog_artifact_readable": _first_non_none(
            primary.get("catalog_artifact_readable"),
            fallback.get("catalog_flow_catalog_artifact_readable"),
        ),
        "catalog_artifact_read_status": _first_non_none(
            primary.get("catalog_artifact_read_status"),
            fallback.get("catalog_flow_catalog_artifact_read_status"),
        ),
        "filter_decision": _first_non_none(
            primary.get("filter_decision"),
            fallback.get("catalog_flow_filter_decision"),
        ),
        "current_repo_actions": list_value(
            primary.get("current_repo_actions")
        ),
        "target_machine_actions": list_value(
            primary.get("target_machine_actions")
        ),
        "claim_boundary": _first_non_none(
            primary.get("claim_boundary"),
            "Evidence Query catalog-flow filter summarizes Catalog evidence only; it does not execute repairs or create benchmark evidence.",
        ),
    }
    for field in CATALOG_FLOW_FILTER_GOVERNANCE_FIELDS:
        projection[field.name] = _first_non_none(
            primary.get(field.name),
            fallback.get(field.name),
        )
    return projection


def project_release_evidence_query_catalog_flow_scorecard(
    release_summary: Mapping[str, Any],
    closure_filter: Mapping[str, Any],
    query_filter: Mapping[str, Any],
) -> Dict[str, Any]:
    """Project release-facing Catalog Flow fields with one source order."""
    release = _mapping_value(release_summary)
    closure = _mapping_value(closure_filter)
    query = _mapping_value(query_filter)
    return {
        f"evidence_query_{field.name}": _first_non_none(
            release.get(f"evidence_query_{field.name}"),
            closure.get(field.name),
            query.get(field.name),
        )
        for field in CATALOG_FLOW_RELEASE_SCORECARD_FIELDS
    }


@dataclass(frozen=True)
class FinalReportReviewSourceLocationProjection:
    scorecard_fields: Mapping[str, int]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, int]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_review_source_location(
    root_cause_review: Mapping[str, Any],
    evidence_query_repair_path_review: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
) -> FinalReportReviewSourceLocationProjection:
    """Project remaining review-facing source-location blockers once."""
    root = _mapping_value(root_cause_review)
    query = _mapping_value(evidence_query_repair_path_review)
    root_source_location_gap_count = max(
        count_coercer(root.get("repair_card_count"))
        - count_coercer(root.get("source_location_card_count")),
        0,
    )
    return FinalReportReviewSourceLocationProjection(
        scorecard_fields={},
        blocker_counts={
            "root_cause_source_location_gaps": root_source_location_gap_count,
            "evidence_query_repair_path_source_gaps": count_coercer(
                query.get("repair_path_source_gap_count")
            ),
            "evidence_query_repair_path_source_locations": count_coercer(
                query.get("repair_path_source_location_count")
            ),
            "evidence_query_repair_path_source_location_gaps": count_coercer(
                query.get("repair_path_source_location_gap_count")
            ),
        },
    )


NATIVE_CORE_AUDIT_REVIEW_MANIFEST_FIELDS = (
    DomainFieldSpec("review_manifest_configured", DOMAIN_VALUE_TRUTHY, False),
    DomainFieldSpec("review_manifest_present", DOMAIN_VALUE_TRUTHY, False),
    DomainFieldSpec("review_manifest_parse_ok", DOMAIN_VALUE_TRUTHY, False),
    DomainFieldSpec("review_manifest_override_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec(
        "review_manifest_applied_override_count", DOMAIN_VALUE_COUNT, 0
    ),
    DomainFieldSpec(
        "review_manifest_invalid_override_count", DOMAIN_VALUE_COUNT, 0
    ),
    DomainFieldSpec(
        "review_manifest_unmatched_override_count", DOMAIN_VALUE_COUNT, 0
    ),
    DomainFieldSpec("review_manifest_closed_file_count", DOMAIN_VALUE_COUNT, 0),
)


NATIVE_CORE_AUDIT_REVIEW_COVERAGE_FIELDS = (
    DomainFieldSpec("audit_review_coverage_state", default="unknown"),
    DomainFieldSpec("audit_review_coverage_open_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_review_coverage_high_priority_open_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_review_coverage_current_repo_action_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_review_coverage_target_machine_action_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_review_coverage_source_location_gap_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_review_coverage_primary_blocker", default="unknown"),
    DomainFieldSpec("audit_review_coverage_primary_environment", default="unknown"),
    DomainFieldSpec("audit_review_coverage_first_open_file", default="unknown"),
    DomainFieldSpec("audit_review_coverage_next_action", default="unknown"),
    DomainFieldSpec(
        "audit_review_coverage_blocks_full_audit_closure",
        DOMAIN_VALUE_TRUTHY,
        False,
    ),
)

NATIVE_CORE_AUDIT_CORE_FIELDS = (
    DomainFieldSpec("source_root_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("source_root_issue_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("source_root_error_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("scope_blocker_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("empty_scan", DOMAIN_VALUE_TRUTHY, False),
    DomainFieldSpec("scan_status", default="unknown"),
    *NATIVE_CORE_AUDIT_REVIEW_MANIFEST_FIELDS,
    DomainFieldSpec("in_scope_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("subsystem_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("reviewed_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("reviewed_with_fix_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("deferred_with_reason_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("issue_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("source_unreadable_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("file_read_status_ok_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("file_read_status_empty_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("file_read_status_non_ok_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("file_read_error_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("path_redaction_review_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("checked_arithmetic_review_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("artifact_contract_review_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("closed_review_file_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("open_review_item_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("high_priority_review_item_count", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_completion_percent", DOMAIN_VALUE_COUNT, 0),
    DomainFieldSpec("audit_progress_state", default="unknown"),
    *NATIVE_CORE_AUDIT_REVIEW_COVERAGE_FIELDS,
    DomainFieldSpec("top_review_priority", default="unknown"),
    DomainFieldSpec("audit_decision", default="unknown"),
    DomainFieldSpec("evidence_confidence", default="unknown"),
)


@dataclass(frozen=True)
class EvidenceQueryNativeAuditFilterFieldSpec:
    name: str
    query_summary_name: Optional[str] = None
    audit_summary_name: Optional[str] = None
    include_audit_summary: bool = True
    additional_sources: Tuple[Tuple[str, str], ...] = ()


def _native_audit_filter_specs(
    fields: Sequence[DomainFieldSpec],
) -> Tuple[EvidenceQueryNativeAuditFilterFieldSpec, ...]:
    return tuple(
        EvidenceQueryNativeAuditFilterFieldSpec(field.name) for field in fields
    )


NATIVE_CORE_AUDIT_FILTER_FIELDS = (
    *_native_audit_filter_specs(NATIVE_CORE_AUDIT_CORE_FIELDS),
    EvidenceQueryNativeAuditFilterFieldSpec(
        "read_issue_count",
        query_summary_name="native_core_audit_filter_read_issue_count",
        include_audit_summary=False,
    ),
    EvidenceQueryNativeAuditFilterFieldSpec(
        "audit_blocker_count",
        query_summary_name="native_core_audit_blocker_count",
        include_audit_summary=False,
    ),
    EvidenceQueryNativeAuditFilterFieldSpec(
        "filter_decision",
        audit_summary_name="audit_decision",
    ),
)


def _validate_native_audit_filter_fields(
    fields: Sequence[EvidenceQueryNativeAuditFilterFieldSpec],
) -> None:
    valid_sources = {"filter", "query_summary", "audit_summary"}
    seen = set()
    for field in fields:
        if not field.name:
            raise ValueError("native audit filter contains an empty field name")
        if field.name in seen:
            raise ValueError(
                f"native audit filter contains duplicate field {field.name}"
            )
        if field.query_summary_name == "":
            raise ValueError(
                f"native audit filter field {field.name} has an empty query source"
            )
        if field.audit_summary_name == "":
            raise ValueError(
                f"native audit filter field {field.name} has an empty audit source"
            )
        for source_name, source_field in field.additional_sources:
            if source_name not in valid_sources or not source_field:
                raise ValueError(
                    f"native audit filter field {field.name} has an invalid additional source"
                )
        seen.add(field.name)


_validate_native_audit_filter_fields(NATIVE_CORE_AUDIT_FILTER_FIELDS)


def project_evidence_query_native_core_audit_filter(
    filter_view: Mapping[str, Any],
    query_summary: Mapping[str, Any],
    native_audit_summary: Mapping[str, Any],
    *,
    direct_query_artifact_present: bool,
    direct_audit_artifact_present: bool,
) -> Dict[str, Any]:
    primary = _mapping_value(filter_view)
    query = _mapping_value(query_summary)
    audit = _mapping_value(native_audit_summary)
    sources = {
        "filter": primary,
        "query_summary": query,
        "audit_summary": audit,
    }
    projection: Dict[str, Any] = {
        "source": "native_evidence_query_result.json",
        "direct_artifact_present": direct_query_artifact_present,
        "filter_query_present": _first_non_none(
            query.get("native_core_audit_filter_query_present"),
            bool(primary),
        ),
        "source_artifact": primary.get(
            "source_artifact", "native_core_audit_register.json"
        ),
        "audit_register_direct_present": direct_audit_artifact_present,
        "audit_register_present": _first_non_none(
            primary.get("audit_register_present"),
            direct_audit_artifact_present,
        ),
        "audit_register_readable": primary.get("audit_register_readable"),
        "audit_register_read_status": primary.get(
            "audit_register_read_status"
        ),
        "audit_summary_present": _first_non_none(
            primary.get("audit_summary_present"), bool(audit)
        ),
    }
    for field in NATIVE_CORE_AUDIT_FILTER_FIELDS:
        query_name = (
            field.query_summary_name or f"native_core_audit_{field.name}"
        )
        candidates = [primary.get(field.name), query.get(query_name)]
        if field.include_audit_summary:
            candidates.append(
                audit.get(field.audit_summary_name or field.name)
            )
        candidates.extend(
            sources[source_name].get(source_field)
            for source_name, source_field in field.additional_sources
        )
        projection[field.name] = _first_non_none(*candidates)
    projection["claim_boundary"] = _first_non_none(
        primary.get("claim_boundary"),
        "Evidence-query native-core audit filter summarizes static audit-register evidence only; it does not compile native code or prove runtime correctness.",
    )
    return projection


_FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_COUNT_FIELD_SOURCES = (
    ("blocker_count", "audit_blocker_count"),
    ("source_root_count", "source_root_count"),
    ("source_root_issue_count", "source_root_issue_count"),
    ("source_root_error_count", "source_root_error_count"),
    ("scope_blocker_count", "scope_blocker_count"),
    ("in_scope_file_count", "in_scope_file_count"),
    ("reviewed_file_count", "reviewed_file_count"),
    ("reviewed_with_fix_file_count", "reviewed_with_fix_file_count"),
    ("deferred_with_reason_file_count", "deferred_with_reason_file_count"),
    ("issue_count", "issue_count"),
    ("source_unreadable_count", "source_unreadable_count"),
    ("review_manifest_invalid_override_count", "review_manifest_invalid_override_count"),
    ("review_manifest_unmatched_override_count", "review_manifest_unmatched_override_count"),
    ("closed_review_file_count", "closed_review_file_count"),
    ("open_review_item_count", "open_review_item_count"),
    ("high_priority_review_item_count", "high_priority_review_item_count"),
    ("audit_completion_percent", "audit_completion_percent"),
    (
        "audit_review_coverage_open_file_count",
        "audit_review_coverage_open_file_count",
    ),
    (
        "audit_review_coverage_high_priority_open_file_count",
        "audit_review_coverage_high_priority_open_file_count",
    ),
    (
        "audit_review_coverage_current_repo_action_count",
        "audit_review_coverage_current_repo_action_count",
    ),
    (
        "audit_review_coverage_target_machine_action_count",
        "audit_review_coverage_target_machine_action_count",
    ),
    (
        "audit_review_coverage_source_location_gap_count",
        "audit_review_coverage_source_location_gap_count",
    ),
    ("read_issue_count", "read_issue_count"),
)

_FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_BOOLEAN_SOURCE_NAMES = (
    "empty_scan",
    "review_manifest_configured",
    "review_manifest_present",
    "review_manifest_parse_ok",
    "audit_review_coverage_blocks_full_audit_closure",
)

_FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_TEXT_SOURCE_NAMES = (
    "scan_status",
    "audit_progress_state",
    "audit_review_coverage_state",
    "audit_review_coverage_primary_blocker",
    "audit_review_coverage_primary_environment",
    "audit_review_coverage_first_open_file",
    "audit_review_coverage_next_action",
    "top_review_priority",
    "audit_decision",
    "evidence_confidence",
    "filter_decision",
)

FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_SCORECARD_FIELDS = (
    tuple(
        DomainFieldSpec(
            f"evidence_query_native_core_audit_{output_name}",
            DOMAIN_VALUE_COUNT,
            0,
            source_name,
        )
        for output_name, source_name in (
            _FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_COUNT_FIELD_SOURCES
        )
    )
    + tuple(
        DomainFieldSpec(
            f"evidence_query_native_core_audit_{source_name}",
            DOMAIN_VALUE_TRUTHY,
            False,
            source_name,
        )
        for source_name in (
            _FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_BOOLEAN_SOURCE_NAMES
        )
    )
    + tuple(
        DomainFieldSpec(
            f"evidence_query_native_core_audit_{source_name}",
            default="unknown",
            source_name=source_name,
        )
        for source_name in (
            _FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_TEXT_SOURCE_NAMES
        )
    )
)

_FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_BLOCKER_ALIAS_SOURCES = (
    ("evidence_query_native_core_audit_blockers", "blocker_count"),
    (
        "evidence_query_native_core_audit_deferred_files",
        "deferred_with_reason_file_count",
    ),
    ("evidence_query_native_core_audit_issues", "issue_count"),
    (
        "evidence_query_native_core_audit_source_unreadable",
        "source_unreadable_count",
    ),
    (
        "evidence_query_native_core_audit_manifest_invalid_overrides",
        "review_manifest_invalid_override_count",
    ),
    (
        "evidence_query_native_core_audit_manifest_unmatched_overrides",
        "review_manifest_unmatched_override_count",
    ),
    (
        "evidence_query_native_core_audit_open_review_items",
        "open_review_item_count",
    ),
    (
        "evidence_query_native_core_audit_high_priority_review_items",
        "high_priority_review_item_count",
    ),
    (
        "evidence_query_native_core_audit_coverage_source_location_gaps",
        "audit_review_coverage_source_location_gap_count",
    ),
    ("evidence_query_native_core_audit_read_issues", "read_issue_count"),
)

_final_report_evidence_query_native_audit_output_names = tuple(
    field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_SCORECARD_FIELDS
)
if len(_final_report_evidence_query_native_audit_output_names) != len(
    set(_final_report_evidence_query_native_audit_output_names)
):
    raise ValueError(
        "Final Report Evidence Query Native Audit scorecard outputs must be unique"
    )

_final_report_evidence_query_native_audit_blocker_aliases = tuple(
    alias
    for alias, _ in (
        _FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_BLOCKER_ALIAS_SOURCES
    )
)
if len(_final_report_evidence_query_native_audit_blocker_aliases) != len(
    set(_final_report_evidence_query_native_audit_blocker_aliases)
):
    raise ValueError(
        "Final Report Evidence Query Native Audit blocker aliases must be unique"
    )

if any(
    f"evidence_query_native_core_audit_{source_name}"
    not in _final_report_evidence_query_native_audit_output_names
    for _, source_name in (
        _FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_BLOCKER_ALIAS_SOURCES
    )
):
    raise ValueError(
        "Final Report Evidence Query Native Audit blocker aliases contain unknown sources"
    )

NATIVE_CORE_AUDIT_PROMETHEUS_METRIC_NAMES = (
    "source_root_count",
    "source_root_issue_count",
    "source_root_error_count",
    "scope_blocker_count",
    "in_scope_file_count",
    "subsystem_count",
    "reviewed_file_count",
    "reviewed_with_fix_file_count",
    "deferred_with_reason_file_count",
    "issue_count",
    "source_unreadable_count",
    "path_redaction_review_count",
    "checked_arithmetic_review_count",
    "artifact_contract_review_count",
    "high_priority_review_item_count",
    "closed_review_file_count",
    "open_review_item_count",
    "audit_completion_percent",
    *(field.name for field in NATIVE_CORE_AUDIT_REVIEW_MANIFEST_FIELDS),
    *(
        field.name
        for field in NATIVE_CORE_AUDIT_REVIEW_COVERAGE_FIELDS
        if field.value_kind
        in {
            DOMAIN_VALUE_COUNT,
            DOMAIN_VALUE_TRUTHY,
            DOMAIN_VALUE_NUMERIC_BOOL,
        }
    ),
)

if len(set(NATIVE_CORE_AUDIT_PROMETHEUS_METRIC_NAMES)) != len(
    NATIVE_CORE_AUDIT_PROMETHEUS_METRIC_NAMES
):
    raise ValueError("native core audit Prometheus metric names must be unique")


RELEASE_PIPELINE_CATALOG_FLOW_DIRECT_REPAIR_PATH_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        (
            "catalog_flow_repair_path_count",
            "catalog_flow_field_coverage_repair_path_count",
            "catalog_flow_fallback_removal_shortest_path_repair_path_count",
            "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count",
            "catalog_flow_fallback_removal_parity_gate_repair_path_count",
            "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
        )
    )
)

RELEASE_PIPELINE_REPORT_DIRECT_REPAIR_PATH_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        (
            "evidence_query_snapshot_diff_repair_path_count",
        )
    )
)

RELEASE_PIPELINE_PRODUCT_REPLAY_DIRECT_REPAIR_PATH_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        (
            "product_replay_semantic_linkage_repair_path_count",
            "product_replay_semantic_linkage_repair_path_source_location_gap_count",
        )
    )
)

RELEASE_PIPELINE_NATIVE_AUDIT_DIRECT_REPAIR_PATH_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        (
            "native_core_audit_repair_path_count",
            "native_core_audit_repair_path_source_location_gap_count",
            "native_core_audit_scope_repair_path_count",
        )
    )
)

RELEASE_PIPELINE_DIRECT_REPAIR_PATH_SCORECARD_FIELDS = (
    RELEASE_PIPELINE_CATALOG_FLOW_DIRECT_REPAIR_PATH_SCORECARD_FIELDS
    + RELEASE_PIPELINE_REPORT_DIRECT_REPAIR_PATH_SCORECARD_FIELDS
    + RELEASE_PIPELINE_PRODUCT_REPLAY_DIRECT_REPAIR_PATH_SCORECARD_FIELDS
    + RELEASE_PIPELINE_NATIVE_AUDIT_DIRECT_REPAIR_PATH_SCORECARD_FIELDS
)

RELEASE_PIPELINE_REPAIR_PATH_SCORECARD_FIELDS = _release_pipeline_count_fields(
    tuple(
        field.name
        for field in dataclass_fields(ReleasePipelineRepairPathScorecardValues)
    )
)

RELEASE_PIPELINE_ALL_REPAIR_PATH_SCORECARD_FIELDS = (
    RELEASE_PIPELINE_DIRECT_REPAIR_PATH_SCORECARD_FIELDS
    + RELEASE_PIPELINE_REPAIR_PATH_SCORECARD_FIELDS
)

RELEASE_PIPELINE_LINKAGE_SCORECARD_FIELDS = _release_pipeline_count_fields(
    tuple(
        field.name
        for field in dataclass_fields(ReleasePipelineLinkageScorecardValues)
    )
)

RELEASE_PIPELINE_BLOCKER_DRILLDOWN_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineBlockerDrilldownScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_READINESS_PATH_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineReadinessPathCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_SOURCE_EXACTNESS_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineSourceExactnessCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_REPAIR_CARD_NAVIGATION_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineRepairCardNavigationCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_REPAIR_CARD_GROUP_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_CARD_NAVIGATION_COUNT_SCORECARD_FIELDS
    if (field.source_name or "").startswith(
        "evidence_query_repair_card_group_"
    )
)

RELEASE_PIPELINE_REPAIR_CARD_GROUP_HEADLINE_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_CARD_GROUP_COUNT_SCORECARD_FIELDS
    if field.source_name == "evidence_query_repair_card_group_count"
)

RELEASE_PIPELINE_REPAIR_CARD_GROUP_DETAIL_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_CARD_GROUP_COUNT_SCORECARD_FIELDS
    if field
    not in RELEASE_PIPELINE_REPAIR_CARD_GROUP_HEADLINE_COUNT_SCORECARD_FIELDS
)

RELEASE_PIPELINE_REPAIR_CARD_PATH_READINESS_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_CARD_NAVIGATION_COUNT_SCORECARD_FIELDS
    if field not in RELEASE_PIPELINE_REPAIR_CARD_GROUP_COUNT_SCORECARD_FIELDS
)

RELEASE_PIPELINE_SOURCE_LOCATION_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineSourceLocationCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_SOURCE_LOCATION_WORKLIST_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_SOURCE_LOCATION_COUNT_SCORECARD_FIELDS
    if (field.source_name or "").startswith(
        "evidence_query_source_location_worklist_"
    )
)

RELEASE_PIPELINE_SOURCE_LOCATION_CATEGORY_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_SOURCE_LOCATION_COUNT_SCORECARD_FIELDS
    if field not in RELEASE_PIPELINE_SOURCE_LOCATION_WORKLIST_COUNT_SCORECARD_FIELDS
)

RELEASE_PIPELINE_REPAIR_GUIDANCE_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineRepairGuidanceCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_REPAIR_GUIDANCE_NARRATIVE_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_GUIDANCE_COUNT_SCORECARD_FIELDS
    if (field.source_name or "").startswith(
        "evidence_query_repair_narrative_"
    )
)

RELEASE_PIPELINE_REPAIR_GUIDANCE_NAVIGATION_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_GUIDANCE_COUNT_SCORECARD_FIELDS
    if (field.source_name or "").startswith(
        "evidence_query_repair_group_navigation_"
    )
)

RELEASE_PIPELINE_REPAIR_GUIDANCE_GROUP_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_REPAIR_GUIDANCE_COUNT_SCORECARD_FIELDS
    if field
    not in (
        RELEASE_PIPELINE_REPAIR_GUIDANCE_NARRATIVE_COUNT_SCORECARD_FIELDS
        + RELEASE_PIPELINE_REPAIR_GUIDANCE_NAVIGATION_COUNT_SCORECARD_FIELDS
    )
)

RELEASE_PIPELINE_CATALOG_FALLBACK_REMOVAL_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineCatalogFallbackRemovalCountScorecardValues
            )
        )
    )
)

_FINAL_REPORT_CATALOG_FLOW_QUERY_TEXT_DEFAULTS = {
    "catalog_flow_fallback_removal_shortest_path_state": "not_reported",
    "catalog_flow_fallback_removal_shortest_path_primary_environment": (
        "not_applicable"
    ),
    "catalog_flow_fallback_removal_shortest_path_first_surface": "",
    "catalog_flow_fallback_removal_parity_gate_decision": "not_reported",
    "catalog_flow_fallback_removal_parity_gate_first_surface": "",
}
_release_pipeline_catalog_fallback_count_source_names = {
    field.source_name
    for field in RELEASE_PIPELINE_CATALOG_FALLBACK_REMOVAL_COUNT_SCORECARD_FIELDS
}
FINAL_REPORT_CATALOG_FLOW_QUERY_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        name=f"release_pipeline_evidence_query_{field.name}",
        value_kind=field.value_kind,
        default=_FINAL_REPORT_CATALOG_FLOW_QUERY_TEXT_DEFAULTS.get(
            field.name,
            field.default,
        ),
        source_name=f"evidence_query_{field.name}",
    )
    for field in CATALOG_FLOW_RELEASE_SCORECARD_FIELDS
    if f"evidence_query_{field.name}"
    not in _release_pipeline_catalog_fallback_count_source_names
)

FINAL_REPORT_CATALOG_FLOW_ROOT_SCORECARD_FIELDS = (
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_present",
        DOMAIN_VALUE_TRUTHY,
        False,
        "root_cause_catalog_flow_fallback_removal_shortest_path_present",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_state",
        default="not_reported",
        source_name=(
            "root_cause_catalog_flow_fallback_removal_shortest_path_state"
        ),
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_primary_environment",
        default="not_applicable",
        source_name=(
            "root_cause_catalog_flow_fallback_removal_shortest_path_primary_environment"
        ),
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_first_surface",
        default="",
        source_name=(
            "root_cause_catalog_flow_fallback_removal_shortest_path_first_surface"
        ),
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_blocks_removal",
        DOMAIN_VALUE_TRUTHY,
        False,
        "root_cause_catalog_flow_fallback_removal_shortest_path_blocks_removal",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_present",
        DOMAIN_VALUE_TRUTHY,
        False,
        "root_cause_catalog_flow_fallback_removal_parity_gate_present",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_decision",
        default="not_reported",
        source_name=(
            "root_cause_catalog_flow_fallback_removal_parity_gate_decision"
        ),
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_removal_allowed",
        DOMAIN_VALUE_TRUTHY,
        False,
        "root_cause_catalog_flow_fallback_removal_parity_gate_removal_allowed",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
        DOMAIN_VALUE_COUNT,
        0,
        "root_cause_catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_first_surface",
        default="",
        source_name=(
            "root_cause_catalog_flow_fallback_removal_parity_gate_first_surface"
        ),
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_blocks_removal",
        DOMAIN_VALUE_TRUTHY,
        False,
        "root_cause_catalog_flow_fallback_removal_parity_gate_blocks_removal",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_count",
        DOMAIN_VALUE_COUNT,
        0,
        "root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_count",
    ),
    DomainFieldSpec(
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
        DOMAIN_VALUE_COUNT,
        0,
        "root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
    ),
)

_FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_COUNT_PREFIXES = (
    "catalog_flow_single_source_",
    "catalog_flow_surface_",
    "catalog_flow_fallback_removal_queue_",
    "catalog_flow_fallback_removal_shortest_path_",
    "catalog_flow_fallback_removal_parity_gate_",
    "catalog_flow_metadata_",
)
_final_report_root_cause_catalog_flow_governance_count_names = tuple(
    field.name
    for field in CATALOG_FLOW_GOVERNANCE_COUNT_FIELDS
    if field.name in {
        "catalog_flow_discrepancy_count",
        "catalog_flow_input_gap_count",
    }
    or field.name.startswith(
        _FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_COUNT_PREFIXES
    )
)
FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_COUNT_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        name=f"root_cause_{source_name}",
        value_kind=DOMAIN_VALUE_COUNT,
        default=0,
        source_name=source_name,
    )
    for source_name in (
        "catalog_flow_root_cause_count",
        *_final_report_root_cause_catalog_flow_governance_count_names,
        "catalog_flow_fallback_removal_shortest_path_root_cause_count",
        "catalog_flow_fallback_removal_parity_gate_root_cause_count",
    )
)
FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_STATE_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        name=f"root_cause_{field.name}",
        value_kind=field.value_kind,
        default=_FINAL_REPORT_CATALOG_FLOW_QUERY_TEXT_DEFAULTS.get(
            field.name,
            field.default,
        ),
        source_name=field.name,
    )
    for field in (
        CATALOG_FLOW_GOVERNANCE_TRUTHY_FIELDS
        + CATALOG_FLOW_GOVERNANCE_TEXT_FIELDS
    )
    if field.name.startswith(
        (
            "catalog_flow_fallback_removal_shortest_path_",
            "catalog_flow_fallback_removal_parity_gate_",
        )
    )
)

_FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_COUNT_PREFIXES = (
    "catalog_flow_single_source_",
    "catalog_flow_surface_",
    "catalog_flow_fallback_removal_queue_",
    "catalog_flow_fallback_removal_shortest_path_",
    "catalog_flow_consumer_field_coverage_",
    "catalog_flow_metadata_",
)
FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_COUNT_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        name=f"evidence_query_{field.name}",
        value_kind=DOMAIN_VALUE_COUNT,
        default=0,
        source_name=field.name,
    )
    for field in CATALOG_FLOW_FILTER_GOVERNANCE_FIELDS
    if field.value_kind == DOMAIN_VALUE_COUNT
    and (
        field.name in {
            "catalog_flow_discrepancy_count",
            "catalog_flow_input_gap_count",
            "catalog_flow_repair_card_count",
        }
        or field.name.startswith(
            _FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_COUNT_PREFIXES
        )
    )
)
FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_STATE_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        name=f"evidence_query_{field.name}",
        value_kind=field.value_kind,
        default=_FINAL_REPORT_CATALOG_FLOW_QUERY_TEXT_DEFAULTS.get(
            field.name,
            field.default,
        ),
        source_name=field.name,
    )
    for field in CATALOG_FLOW_FILTER_GOVERNANCE_FIELDS
    if field.name.startswith(
        "catalog_flow_fallback_removal_shortest_path_"
    )
    and field.value_kind != DOMAIN_VALUE_COUNT
) + (
    DomainFieldSpec(
        "evidence_query_catalog_flow_filter_decision",
        default="unknown",
        source_name="filter_decision",
    ),
)

_FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_COUNT_SOURCE_NAMES = (
    "selected_result_present",
    "selected_result_input_ready",
    "selected_result_payload_count",
    "show_artifact_candidate_count",
    "show_artifact_candidate_truncated",
    "show_artifact_exact_match",
    "show_artifact_selector_ambiguous",
    "payload_selector_focus_present",
    "payload_selector_focus_match_count",
    "payload_selector_focus_ambiguous",
    "payload_selector_focus_source_location_gap_count",
    "selector_match_count",
    "selector_ambiguous",
    "selector_repair_card_count",
    "selector_repair_card_emitted_count",
    "selector_repair_card_truncated",
    "selector_repair_card_embedded_limit",
    "selector_repair_card_full_export_count",
    "selector_repair_card_external_export_configured",
    "selector_repair_card_external_export_written",
    "selector_repair_card_current_repo_count",
    "selector_repair_card_current_server_count",
    "selector_repair_card_target_machine_count",
    "selector_repair_card_target_product_count",
    "selector_repair_card_source_location_gap_card_count",
    "selector_repair_card_external_file_required",
    "selector_source_location_count",
    "selector_source_location_gap_count",
    "selector_current_repo_action_count",
    "selector_current_server_action_count",
    "selector_target_machine_action_count",
    "selector_target_product_action_count",
    "selector_action_plan_present",
    "selector_action_plan_step_count",
    "selector_action_plan_current_repo_step_count",
    "selector_action_plan_target_or_validation_step_count",
    "selector_action_plan_source_location_gap_count",
    "selector_action_plan_readiness_blocker_count",
)

_FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_TEXT_SOURCE_NAMES = (
    "selected_result_kind",
    "show_artifact_selection_strategy",
    "payload_selector_focus_status",
    "selector_resolution_status",
    "selector_kind",
    "selector_repair_card_export_status",
    "selector_repair_card_external_export_status",
    "selector_repair_card_export_review_status",
    "selector_repair_card_export_blocker_reason",
    "selector_action_plan_decision",
    "selector_action_plan_readiness_state",
)

FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        f"evidence_query_ux_{source_name}",
        DOMAIN_VALUE_COUNT,
        0,
        source_name,
    )
    for source_name in _FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_COUNT_SOURCE_NAMES
) + tuple(
    DomainFieldSpec(
        f"evidence_query_ux_{source_name}",
        default="unknown",
        source_name=source_name,
    )
    for source_name in _FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_TEXT_SOURCE_NAMES
)

_FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_COUNT_SOURCE_NAMES = (
    "operation_catalog_supported_count",
    "operation_catalog_read_only_count",
    "operation_catalog_snapshot_required_count",
    "operation_catalog_selector_recommended_count",
    "operation_catalog_target_required_count",
    "snapshot_diff_read_issue_count",
    "snapshot_diff_content_compared",
    "snapshot_diff_content_changed",
    "snapshot_diff_semantic_compared",
    "snapshot_diff_deep_semantic_compared",
    "snapshot_diff_deep_semantic_truncated",
    "snapshot_diff_added_top_level_key_count",
    "snapshot_diff_removed_top_level_key_count",
    "snapshot_diff_changed_top_level_value_count",
    "snapshot_diff_added_deep_path_count",
    "snapshot_diff_removed_deep_path_count",
    "snapshot_diff_changed_deep_value_count",
    "snapshot_diff_size_delta_bytes",
    "snapshot_to_release_focus_present",
    "snapshot_to_release_focus_review_required",
    "snapshot_to_release_focus_current_repo_action_count",
    "snapshot_to_release_focus_changed_deep_path_count",
)

FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_SCORECARD_FIELDS = (
    DomainFieldSpec(
        "evidence_query_ux_operation",
        default="unknown",
        source_name="operation",
    ),
    DomainFieldSpec(
        "evidence_query_ux_operation_human_label",
        default="unknown",
        source_name="operation_human_label",
    ),
    DomainFieldSpec(
        "evidence_query_ux_operation_target",
        default="unknown",
        source_name="operation_target",
    ),
    DomainFieldSpec(
        "evidence_query_ux_operation_target_present",
        source_name="operation_target_present",
    ),
    DomainFieldSpec(
        "evidence_query_ux_operation_snapshot_inputs_required",
        source_name="operation_snapshot_inputs_required",
    ),
    DomainFieldSpec(
        "evidence_query_ux_operation_snapshot_inputs_configured",
        source_name="operation_snapshot_inputs_configured",
    ),
    *(
        DomainFieldSpec(
            f"evidence_query_{source_name}",
            DOMAIN_VALUE_COUNT,
            0,
            source_name,
        )
        for source_name in (
            _FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_COUNT_SOURCE_NAMES
        )
    ),
    DomainFieldSpec(
        "evidence_query_snapshot_diff_decision",
        default="unknown",
        source_name="snapshot_diff_decision",
    ),
    DomainFieldSpec(
        "evidence_query_snapshot_diff_semantic_decision",
        default="unknown",
        source_name="snapshot_diff_semantic_decision",
    ),
    DomainFieldSpec(
        "evidence_query_snapshot_diff_deep_semantic_decision",
        default="unknown",
        source_name="snapshot_diff_deep_semantic_decision",
    ),
    DomainFieldSpec(
        "evidence_query_snapshot_to_release_focus_link_status",
        default="unknown",
        source_name="snapshot_to_release_focus_link_status",
    ),
    DomainFieldSpec(
        "evidence_query_snapshot_to_release_focus_blocker_reason",
        default="unknown",
        source_name="snapshot_to_release_focus_blocker_reason",
    ),
)

_final_report_evidence_query_ux_operation_snapshot_output_names = tuple(
    field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_SCORECARD_FIELDS
)
if len(_final_report_evidence_query_ux_operation_snapshot_output_names) != 33 or len(
    _final_report_evidence_query_ux_operation_snapshot_output_names
) != len(set(_final_report_evidence_query_ux_operation_snapshot_output_names)):
    raise ValueError(
        "Final Report Evidence Query operation/snapshot scorecard must contain 33 unique fields"
    )

_FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_COUNT_SOURCE_NAMES = (
    "repair_card_count",
    "repair_card_group_count",
    "repair_card_group_card_count",
    "repair_card_group_current_repo_count",
    "repair_card_group_current_server_count",
    "repair_card_group_target_machine_count",
    "repair_card_group_target_product_count",
    "repair_card_group_source_location_gap_count",
    "repair_group_exact_wiring_location_count",
    "repair_group_affected_claim_count",
    "repair_group_navigation_step_count",
    "repair_group_navigation_current_repo_step_count",
    "repair_group_navigation_target_or_validation_step_count",
    "source_location_worklist_item_count",
    "source_location_worklist_current_repo_item_count",
    "source_location_worklist_target_or_validation_item_count",
    "source_location_worklist_gap_count",
    "source_location_worklist_focus_present",
    "source_location_worklist_focus_first_item_present",
    "source_location_worklist_focus_first_gap_present",
    "source_location_worklist_focus_first_item_current_repo_action_required",
    "source_location_worklist_focus_first_item_target_or_validation_action_required",
    "source_location_category_count",
    "source_location_active_category_count",
    "source_location_exact_category_count",
    "source_location_gap_category_count",
    "source_location_current_repo_gap_category_count",
    "source_location_target_or_validation_gap_category_count",
    "source_location_gap_repair_card_count",
    "source_location_category_exactness_percent",
    "repair_card_path_rank_count",
    "repair_card_path_current_repo_rank_count",
    "repair_card_path_target_or_validation_rank_count",
    "repair_card_path_source_location_gap_count",
    "repair_card_rank_readiness_link_count",
    "repair_card_rank_readiness_current_repo_link_count",
    "repair_card_rank_readiness_target_or_validation_link_count",
    "repair_card_rank_readiness_report_ready_link_count",
    "repair_card_rank_readiness_replay_ready_link_count",
    "repair_card_rank_readiness_execute_ready_link_count",
    "repair_card_source_exactness_card_count",
    "repair_card_source_exactness_exact_card_count",
    "repair_card_source_exactness_gap_card_count",
    "repair_card_source_exactness_current_repo_gap_count",
    "repair_card_source_exactness_target_or_validation_gap_count",
    "repair_card_source_exactness_percent",
    "explain_tree_node_count",
    "explain_tree_source_exact_node_count",
    "explain_tree_source_gap_node_count",
    "explain_tree_current_repo_source_gap_count",
    "explain_tree_target_or_validation_source_gap_count",
    "explain_tree_source_exactness_percent",
    "blocker_drilldown_record_count",
    "blocker_drilldown_category_count",
    "blocker_drilldown_source_location_gap_count",
    "blocker_drilldown_affected_claim_count",
    "blocker_drilldown_exact_wiring_location_count",
    "blocker_drilldown_current_repo_action_count",
    "blocker_drilldown_current_server_action_count",
    "blocker_drilldown_target_machine_action_count",
    "blocker_drilldown_target_product_action_count",
    "blocker_category_count",
    "exact_wiring_location_count",
    "report_ready_blocker_count",
    "replay_ready_blocker_count",
    "execute_ready_blocker_count",
)

_FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_NUMERIC_BOOLEAN_SOURCE_NAMES = (
    "repair_group_primary_action_present",
    "repair_group_primary_source_location_present",
)

_FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_TEXT_SOURCE_NAMES = (
    "repair_group_primary_environment",
    "repair_group_primary_category",
    "repair_group_primary_artifact",
)

FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_SCORECARD_FIELDS = tuple(
    DomainFieldSpec(
        f"evidence_query_ux_{source_name}",
        DOMAIN_VALUE_COUNT,
        0,
        source_name,
    )
    for source_name in (
        _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_COUNT_SOURCE_NAMES
        + _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_NUMERIC_BOOLEAN_SOURCE_NAMES
    )
) + tuple(
    DomainFieldSpec(
        f"evidence_query_ux_{source_name}",
        default="unknown",
        source_name=source_name,
    )
    for source_name in _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_TEXT_SOURCE_NAMES
)

_FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_BLOCKER_ALIAS_SOURCES = (
    ("evidence_query_ux_repair_cards", "repair_card_count"),
    ("evidence_query_ux_repair_card_groups", "repair_card_group_count"),
    ("evidence_query_ux_repair_card_group_cards", "repair_card_group_card_count"),
    ("evidence_query_ux_repair_card_group_current_repo", "repair_card_group_current_repo_count"),
    ("evidence_query_ux_repair_card_group_current_server", "repair_card_group_current_server_count"),
    ("evidence_query_ux_repair_card_group_target_machine", "repair_card_group_target_machine_count"),
    ("evidence_query_ux_repair_card_group_target_product", "repair_card_group_target_product_count"),
    ("evidence_query_ux_repair_card_group_source_location_gaps", "repair_card_group_source_location_gap_count"),
    ("evidence_query_ux_repair_group_exact_wiring_locations", "repair_group_exact_wiring_location_count"),
    ("evidence_query_ux_repair_group_affected_claims", "repair_group_affected_claim_count"),
    ("evidence_query_ux_repair_group_primary_action_present", "repair_group_primary_action_present"),
    ("evidence_query_ux_repair_group_primary_source_location_present", "repair_group_primary_source_location_present"),
    ("evidence_query_ux_repair_group_navigation_steps", "repair_group_navigation_step_count"),
    ("evidence_query_ux_repair_group_navigation_current_repo_steps", "repair_group_navigation_current_repo_step_count"),
    ("evidence_query_ux_repair_group_navigation_target_or_validation_steps", "repair_group_navigation_target_or_validation_step_count"),
    ("evidence_query_ux_source_location_worklist_items", "source_location_worklist_item_count"),
    ("evidence_query_ux_source_location_worklist_current_repo_items", "source_location_worklist_current_repo_item_count"),
    ("evidence_query_ux_source_location_worklist_target_or_validation_items", "source_location_worklist_target_or_validation_item_count"),
    ("evidence_query_ux_source_location_worklist_gaps", "source_location_worklist_gap_count"),
    ("evidence_query_ux_source_location_categories", "source_location_category_count"),
    ("evidence_query_ux_source_location_active_categories", "source_location_active_category_count"),
    ("evidence_query_ux_source_location_exact_categories", "source_location_exact_category_count"),
    ("evidence_query_ux_source_location_gap_categories", "source_location_gap_category_count"),
    ("evidence_query_ux_source_location_current_repo_gap_categories", "source_location_current_repo_gap_category_count"),
    ("evidence_query_ux_source_location_target_or_validation_gap_categories", "source_location_target_or_validation_gap_category_count"),
    ("evidence_query_ux_source_location_gap_repair_cards", "source_location_gap_repair_card_count"),
    ("evidence_query_ux_source_location_category_exactness_percent", "source_location_category_exactness_percent"),
    ("evidence_query_ux_repair_card_path_ranks", "repair_card_path_rank_count"),
    ("evidence_query_ux_repair_card_path_current_repo_ranks", "repair_card_path_current_repo_rank_count"),
    ("evidence_query_ux_repair_card_path_target_or_validation_ranks", "repair_card_path_target_or_validation_rank_count"),
    ("evidence_query_ux_repair_card_path_source_location_gaps", "repair_card_path_source_location_gap_count"),
    ("evidence_query_ux_repair_card_rank_readiness_links", "repair_card_rank_readiness_link_count"),
    ("evidence_query_ux_repair_card_rank_readiness_current_repo_links", "repair_card_rank_readiness_current_repo_link_count"),
    ("evidence_query_ux_repair_card_rank_readiness_target_or_validation_links", "repair_card_rank_readiness_target_or_validation_link_count"),
    ("evidence_query_ux_repair_card_rank_readiness_report_ready_links", "repair_card_rank_readiness_report_ready_link_count"),
    ("evidence_query_ux_repair_card_rank_readiness_replay_ready_links", "repair_card_rank_readiness_replay_ready_link_count"),
    ("evidence_query_ux_repair_card_rank_readiness_execute_ready_links", "repair_card_rank_readiness_execute_ready_link_count"),
    ("evidence_query_ux_repair_card_source_exactness_cards", "repair_card_source_exactness_card_count"),
    ("evidence_query_ux_repair_card_source_exact_cards", "repair_card_source_exactness_exact_card_count"),
    ("evidence_query_ux_repair_card_source_gap_cards", "repair_card_source_exactness_gap_card_count"),
    ("evidence_query_ux_repair_card_source_current_repo_gaps", "repair_card_source_exactness_current_repo_gap_count"),
    ("evidence_query_ux_repair_card_source_target_or_validation_gaps", "repair_card_source_exactness_target_or_validation_gap_count"),
    ("evidence_query_ux_explain_tree_node_count", "explain_tree_node_count"),
    ("evidence_query_ux_explain_tree_source_exact_node_count", "explain_tree_source_exact_node_count"),
    ("evidence_query_ux_explain_tree_source_gap_node_count", "explain_tree_source_gap_node_count"),
    ("evidence_query_ux_explain_tree_current_repo_source_gap_count", "explain_tree_current_repo_source_gap_count"),
    ("evidence_query_ux_explain_tree_target_or_validation_source_gap_count", "explain_tree_target_or_validation_source_gap_count"),
    ("evidence_query_ux_explain_tree_source_exactness_percent", "explain_tree_source_exactness_percent"),
    ("evidence_query_ux_blocker_drilldown_record_count", "blocker_drilldown_record_count"),
    ("evidence_query_ux_blocker_drilldown_category_count", "blocker_drilldown_category_count"),
    ("evidence_query_ux_blocker_drilldown_source_location_gap_count", "blocker_drilldown_source_location_gap_count"),
    ("evidence_query_ux_blocker_drilldown_affected_claim_count", "blocker_drilldown_affected_claim_count"),
    ("evidence_query_ux_blocker_drilldown_exact_wiring_location_count", "blocker_drilldown_exact_wiring_location_count"),
    ("evidence_query_ux_blocker_drilldown_current_repo_action_count", "blocker_drilldown_current_repo_action_count"),
    ("evidence_query_ux_blocker_drilldown_current_server_action_count", "blocker_drilldown_current_server_action_count"),
    ("evidence_query_ux_blocker_drilldown_target_machine_action_count", "blocker_drilldown_target_machine_action_count"),
    ("evidence_query_ux_blocker_drilldown_target_product_action_count", "blocker_drilldown_target_product_action_count"),
    ("evidence_query_ux_blocker_categories", "blocker_category_count"),
    ("evidence_query_ux_exact_wiring_locations", "exact_wiring_location_count"),
    ("evidence_query_ux_report_ready_blockers", "report_ready_blocker_count"),
    ("evidence_query_ux_replay_ready_blockers", "replay_ready_blocker_count"),
    ("evidence_query_ux_execute_ready_blockers", "execute_ready_blocker_count"),
)

_final_report_evidence_query_ux_review_output_names = tuple(
    field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_SCORECARD_FIELDS
)
if len(_final_report_evidence_query_ux_review_output_names) != len(
    set(_final_report_evidence_query_ux_review_output_names)
):
    raise ValueError(
        "Final Report Evidence Query UX review scorecard outputs must be unique"
    )

_final_report_evidence_query_ux_review_blocker_aliases = tuple(
    alias
    for alias, _ in _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_BLOCKER_ALIAS_SOURCES
)
if len(_final_report_evidence_query_ux_review_blocker_aliases) != len(
    set(_final_report_evidence_query_ux_review_blocker_aliases)
):
    raise ValueError(
        "Final Report Evidence Query UX review blocker aliases must be unique"
    )

_final_report_evidence_query_ux_review_source_names = {
    field.source_name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_SCORECARD_FIELDS
}
if any(
    source_name not in _final_report_evidence_query_ux_review_source_names
    for _, source_name in _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_BLOCKER_ALIAS_SOURCES
):
    raise ValueError(
        "Final Report Evidence Query UX review blocker aliases contain unknown sources"
    )


_FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_OPERATION_FIELDS = (
    "operation_selector_present",
    "operation_target_present",
    "operation_snapshot_inputs_required",
    "operation_snapshot_inputs_configured",
)
_FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_SELECTOR_FIELDS = tuple(
    field.source_name or field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_SCORECARD_FIELDS
)
_FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_OPERATION_CATALOG_FIELDS = tuple(
    field.source_name or field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_SCORECARD_FIELDS
    if (field.source_name or field.name).startswith("operation_catalog_")
)
_FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_SNAPSHOT_FIELDS = tuple(
    field.source_name or field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_SCORECARD_FIELDS
    if (field.source_name or field.name).startswith("snapshot_")
)
_FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_REVIEW_FIELDS = tuple(
    field.source_name or field.name
    for field in FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_SCORECARD_FIELDS
) + (
    "repair_group_summary_present",
    "source_location_worklist_focus_first_item_environment",
    "source_location_worklist_focus_first_item_artifact",
    "source_location_worklist_focus_first_item_category",
    "source_location_worklist_focus_first_item_source_location",
    "source_location_worklist_focus_first_gap_environment",
    "source_location_worklist_focus_first_gap_category",
    "source_location_worklist_focus_next_action",
)
_FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_CROSS_ARTIFACT_FIELDS = tuple(
    name
    for name in _EVIDENCE_QUERY_UX_CROSS_ARTIFACT_FIELDS
    if not name.startswith("cross_artifact_release_focus_")
)
_final_report_evidence_query_ux_closure_field_names = (
    _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_OPERATION_FIELDS
    + _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_SELECTOR_FIELDS
    + _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_OPERATION_CATALOG_FIELDS
    + _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_REVIEW_FIELDS
    + _EVIDENCE_QUERY_UX_READINESS_PATH_FIELDS
    + _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_CROSS_ARTIFACT_FIELDS
    + _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_SNAPSHOT_FIELDS
)
if len(_final_report_evidence_query_ux_closure_field_names) != len(
    set(_final_report_evidence_query_ux_closure_field_names)
):
    raise ValueError("Final Report Evidence Query UX closure fields must be unique")


def project_final_report_evidence_query_ux_closure(
    evidence_query_summary: Mapping[str, Any],
    *,
    direct_artifact_present: bool,
    operation: Any,
    operation_human_label: Any,
    operation_target: Any,
    claim_boundary: Any,
) -> Dict[str, Any]:
    """Project the read-only Evidence Query UX closure from canonical field views."""
    summary = _mapping_value(evidence_query_summary)
    projection: Dict[str, Any] = {
        "source": "native_evidence_query_result.json",
        "direct_artifact_present": direct_artifact_present,
        "operation": operation,
        "operation_human_label": operation_human_label,
        "operation_target": operation_target,
    }
    projection.update(
        {
            name: summary.get(f"query_{name}")
            for name in _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_OPERATION_FIELDS
        }
    )
    projection.update(
        {
            name: summary.get(f"query_operation_{name}")
            for name in _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_SELECTOR_FIELDS
        }
    )
    projection.update(
        {
            name: summary.get(name)
            for name in _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_OPERATION_CATALOG_FIELDS
        }
    )
    projection.update(
        {
            name: summary.get(f"query_ux_{name}")
            for name in (
                _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_REVIEW_FIELDS
                + _EVIDENCE_QUERY_UX_READINESS_PATH_FIELDS
            )
        }
    )
    projection.update(
        {
            name: summary.get(name)
            for name in (
                _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_CROSS_ARTIFACT_FIELDS
                + _FINAL_REPORT_EVIDENCE_QUERY_UX_CLOSURE_SNAPSHOT_FIELDS
            )
        }
    )
    projection["claim_boundary"] = claim_boundary
    return projection


_FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_SHARED_FIELDS = (
    "release_decision",
    "top_root_cause",
    "root_cause_count",
    "blocking_root_cause_count",
    "current_repo_repair_card_count",
    "target_product_replay_repair_card_count",
    "target_gpu_execute_repair_card_count",
    "target_machine_repair_card_count",
    "repair_environment_split_status",
    "repair_environment_split_schema_gap",
    "repair_environment_legacy_fallback_used",
    "flow_check_repair_card_count",
)
_FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_NATIVE_AUDIT_FIELDS = (
    "native_core_audit_root_cause_count",
    "native_core_audit_backlog_root_cause_count",
    "native_core_audit_scope_root_cause_count",
    "native_core_audit_review_manifest_root_cause_count",
    "native_core_audit_review_manifest_invalid_override_count",
    "native_core_audit_review_manifest_unmatched_override_count",
    "native_core_audit_blocker_count",
    "native_core_audit_source_unreadable_count",
    "native_core_audit_filter_decision",
)
_FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_SOURCE_LOCATION_FIELDS = (
    (
        "source_location_review_present",
        "root_cause_source_location_review_present",
    ),
    ("source_location_card_count", "root_cause_source_location_card_count"),
    (
        "first_card_source_location_count",
        "root_cause_first_card_source_location_count",
    ),
)
_final_report_root_cause_repair_review_output_names = (
    _FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_SHARED_FIELDS
    + _FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_NATIVE_AUDIT_FIELDS
    + tuple(
        output_name
        for output_name, _ in (
            _FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_SOURCE_LOCATION_FIELDS
        )
    )
)
if len(_final_report_root_cause_repair_review_output_names) != len(
    set(_final_report_root_cause_repair_review_output_names)
):
    raise ValueError("Final Report root-cause repair review fields must be unique")


def project_final_report_root_cause_repair_review(
    root_cause_review: Mapping[str, Any],
    root_cause_summary: Mapping[str, Any],
    *,
    direct_artifact_present: bool,
    root_cause_cards: Sequence[Any],
    repair_card_summaries: Sequence[Mapping[str, Any]],
    first_repair_card: Mapping[str, Any],
    catalog_flow_card_stats: Mapping[str, Any],
    catalog_flow_governance: Mapping[str, Any],
    catalog_flow_queue: Mapping[str, Any],
    catalog_flow_shortest_path: Mapping[str, Any],
    catalog_flow_parity_gate: Mapping[str, Any],
    product_replay: Mapping[str, Any],
) -> Dict[str, Any]:
    """Compose the root-cause review without recomputing upstream decisions."""
    review = _mapping_value(root_cause_review)
    root_summary = _mapping_value(root_cause_summary)
    first_card = _mapping_value(first_repair_card)
    card_stats = _mapping_value(catalog_flow_card_stats)

    projection: Dict[str, Any] = {
        "source": "native_evidence_query_result.json",
        "direct_artifact_present": direct_artifact_present,
    }
    projection.update(
        {
            name: first_present(review.get(name), root_summary.get(name))
            for name in _FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_SHARED_FIELDS
        }
    )
    projection["repair_card_count"] = first_present(
        review.get("root_cause_repair_card_count"),
        root_summary.get("root_cause_repair_card_count"),
        len(root_cause_cards) if root_cause_cards else None,
    )
    for fields in (
        catalog_flow_governance,
        catalog_flow_queue,
        catalog_flow_shortest_path,
        catalog_flow_parity_gate,
        product_replay,
    ):
        projection.update(_mapping_value(fields))
    projection.update(
        {
            name: first_present(review.get(name), root_summary.get(name))
            for name in _FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_NATIVE_AUDIT_FIELDS
        }
    )
    projection.update(
        {
            "catalog_flow_source_location_card_count": card_stats.get(
                "source_location_card_count"
            ),
            "catalog_flow_source_location_count": card_stats.get(
                "source_location_count"
            ),
            "catalog_flow_first_source_location": card_stats.get(
                "first_source_location"
            ),
        }
    )
    projection.update(
        {
            output_name: first_present(
                review.get(output_name),
                root_summary.get(summary_name),
            )
            for output_name, summary_name in (
                _FINAL_REPORT_ROOT_CAUSE_REPAIR_REVIEW_SOURCE_LOCATION_FIELDS
            )
        }
    )
    projection.update(
        {
            "first_source_location": review.get("first_source_location"),
            "has_root_cause_repair_cards": first_present(
                review.get("has_root_cause_repair_cards"),
                bool(root_cause_cards),
            ),
            "repair_card_table_count": len(repair_card_summaries),
            "repair_cards": list(repair_card_summaries),
            "first_repair_card": (
                {
                    "card_id": first_card.get("card_id"),
                    "artifact": first_card.get("artifact"),
                    "blocker_category": first_card.get("blocker_category"),
                    "severity": first_card.get("severity"),
                    "priority": first_card.get("priority"),
                    "repair_environment": first_card.get("repair_environment"),
                    "suggested_repair": first_present(
                        first_card.get("suggested_repair"),
                        first_card.get("recommended_action"),
                    ),
                    "flow_check_recommended": first_card.get(
                        "flow_check_recommended"
                    ),
                }
                if first_card
                else {}
            ),
        }
    )
    return projection


def project_final_report_evidence_query_repair_path_review(
    repair_path_query: Mapping[str, Any],
    evidence_query_summary: Mapping[str, Any],
    *,
    direct_artifact_present: bool,
) -> Dict[str, Any]:
    """Project Evidence Query first-action repair guidance without executing it."""
    query = _mapping_value(repair_path_query)
    summary = _mapping_value(evidence_query_summary)
    root_cause = _mapping_value(query.get("root_cause"))
    shortest_paths = list_value(query.get("shortest_paths"))
    first_path = (
        _mapping_value(shortest_paths[0]) if shortest_paths else {}
    )
    projection: Dict[str, Any] = {
        "source": "native_evidence_query_result.json",
        "upstream_artifact": "native_evidence_query_result.json",
        "evidence_query_artifact_present": direct_artifact_present,
        "repair_path_query_present": bool(query),
    }
    projection.update(
        {
            "repair_path_count": first_present(
                root_cause.get("repair_card_count"),
                summary.get("root_cause_repair_card_count"),
            ),
            "repair_path_source_gap_count": query.get("source_gap_count"),
            "repair_path_source_location_count": query.get(
                "source_location_count"
            ),
            "repair_path_source_location_gap_count": query.get(
                "source_location_gap_count"
            ),
            "first_action_present": bool(first_path.get("first_action")),
            "first_target": first_path.get("target"),
            "first_state": first_path.get("state"),
            "first_action": first_path.get("first_action"),
            "review_decision": first_path.get("state"),
            "claim_boundary": first_present(
                query.get("claim_boundary"),
                "Evidence Query repair paths are read-only guidance; they do not execute repairs or create benchmark evidence.",
            ),
        }
    )
    return projection

_final_report_evidence_query_catalog_flow_output_names = tuple(
    field.name
    for field in (
        FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_COUNT_SCORECARD_FIELDS
        + FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_STATE_SCORECARD_FIELDS
    )
)
if len(_final_report_evidence_query_catalog_flow_output_names) != len(
    set(_final_report_evidence_query_catalog_flow_output_names)
):
    raise ValueError(
        "Final Report Evidence Query Catalog Flow scorecard outputs must be unique"
    )

_final_report_root_cause_catalog_flow_output_names = tuple(
    field.name
    for field in (
        FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_COUNT_SCORECARD_FIELDS
        + FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_STATE_SCORECARD_FIELDS
    )
)
if len(_final_report_root_cause_catalog_flow_output_names) != len(
    set(_final_report_root_cause_catalog_flow_output_names)
):
    raise ValueError(
        "Final Report Root Cause Catalog Flow scorecard outputs must be unique"
    )

_final_report_catalog_flow_scorecard_output_names = tuple(
    field.name
    for field in (
        RELEASE_PIPELINE_CATALOG_FALLBACK_REMOVAL_COUNT_SCORECARD_FIELDS
        + FINAL_REPORT_CATALOG_FLOW_QUERY_SCORECARD_FIELDS
        + FINAL_REPORT_CATALOG_FLOW_ROOT_SCORECARD_FIELDS
    )
)
if len(_final_report_catalog_flow_scorecard_output_names) != len(
    set(_final_report_catalog_flow_scorecard_output_names)
):
    raise ValueError("Final Report Catalog Flow scorecard outputs must be unique")

RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineSelectorReplayCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_VIEWS = {
    "selector_match": _domain_fields_by_source_names(
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
        ("evidence_query_payload_selector_focus_match_count",),
    ),
    "selector_source_location_gap": _domain_fields_by_source_names(
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
        ("evidence_query_payload_selector_focus_source_location_gap_count",),
    ),
    "product_replay_blocker": _domain_fields_by_source_names(
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
        ("evidence_query_product_replay_semantic_linkage_blocker_count",),
    ),
    "product_replay_pending_event": _domain_fields_by_source_names(
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
        ("evidence_query_product_replay_semantic_linkage_pending_event_count",),
    ),
    "product_replay_correlation": _domain_fields_by_source_names(
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
        ("evidence_query_product_replay_semantic_linkage_correlation_count",),
    ),
    "product_replay_target_or_validation_action": _domain_fields_by_source_names(
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
        (
            "evidence_query_product_replay_semantic_linkage_target_or_validation_action_count",
        ),
    ),
}

RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS = (
    _release_pipeline_truthy_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineNativeBooleanSignalScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_VIEWS = {
    "query_fallback_removal_blocks": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_catalog_flow_fallback_removal_parity_gate_blocks_removal",
        ),
    ),
    "root_fallback_removal_blocks": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "root_cause_catalog_flow_fallback_removal_parity_gate_blocks_removal",
        ),
    ),
    "readiness_current_repo_actionable": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_readiness_path_current_repo_actionable",),
    ),
    "readiness_validation_or_target_required": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_readiness_path_validation_or_target_required",),
    ),
    "readiness_source_location_metadata_required": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_readiness_path_source_location_metadata_required",
        ),
    ),
    "readiness_next_action_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_readiness_path_next_action_present",),
    ),
    "cross_release_focus_current_repo_actionable": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_cross_artifact_release_focus_current_repo_actionable",
        ),
    ),
    "cross_release_focus_target_or_validation_required": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_cross_artifact_release_focus_target_or_validation_required",
        ),
    ),
    "cross_release_focus_source_location_metadata_required": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_cross_artifact_release_focus_source_location_metadata_required",
        ),
    ),
    "cross_product_replay_family_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_cross_artifact_product_replay_family_present",
        ),
    ),
    "product_replay_sample_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_product_replay_semantic_linkage_sample_replay_present",
        ),
    ),
}

RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS = (
    _release_pipeline_numeric_boolean_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineNumericBooleanSignalScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_VIEWS = {
    "selector_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_payload_selector_focus_present",),
    ),
    "selector_ambiguous": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_payload_selector_focus_ambiguous",),
    ),
    "repair_group_summary_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_repair_group_summary_present",),
    ),
    "repair_narrative_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_repair_narrative_present",),
    ),
    "repair_group_primary_action_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_repair_group_primary_action_present",),
    ),
    "repair_group_primary_source_location_present": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        ("evidence_query_repair_group_primary_source_location_present",),
    ),
    "source_location_worklist_focus": _domain_fields_by_source_names(
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
        (
            "evidence_query_source_location_worklist_focus_present",
            "evidence_query_source_location_worklist_focus_first_item_present",
            "evidence_query_source_location_worklist_focus_first_gap_present",
            "evidence_query_source_location_worklist_focus_first_item_current_repo_action_required",
            "evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required",
        ),
    ),
}

RELEASE_PIPELINE_CROSS_ARTIFACT_COUNT_SCORECARD_FIELDS = (
    _release_pipeline_count_fields(
        tuple(
            field.name
            for field in dataclass_fields(
                ReleasePipelineCrossArtifactCountScorecardValues
            )
        )
    )
)

RELEASE_PIPELINE_CROSS_ARTIFACT_RELEASE_FOCUS_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_CROSS_ARTIFACT_COUNT_SCORECARD_FIELDS
    if (field.source_name or "").startswith(
        "evidence_query_cross_artifact_release_focus_"
    )
)

RELEASE_PIPELINE_CROSS_ARTIFACT_PRODUCT_REPLAY_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_CROSS_ARTIFACT_COUNT_SCORECARD_FIELDS
    if (field.source_name or "").startswith(
        "evidence_query_cross_artifact_product_replay_family_"
    )
)

RELEASE_PIPELINE_CROSS_ARTIFACT_CORE_COUNT_SCORECARD_FIELDS = tuple(
    field
    for field in RELEASE_PIPELINE_CROSS_ARTIFACT_COUNT_SCORECARD_FIELDS
    if field
    not in (
        RELEASE_PIPELINE_CROSS_ARTIFACT_RELEASE_FOCUS_COUNT_SCORECARD_FIELDS
        + RELEASE_PIPELINE_CROSS_ARTIFACT_PRODUCT_REPLAY_COUNT_SCORECARD_FIELDS
    )
)


def _validate_domain_field_specs(
    group_name: str, fields: Sequence[DomainFieldSpec]
) -> None:
    valid_kinds = {
        DOMAIN_VALUE_RAW,
        DOMAIN_VALUE_COUNT,
        DOMAIN_VALUE_TRUTHY,
        DOMAIN_VALUE_NUMERIC_BOOL,
    }
    seen = set()
    for field in fields:
        if not field.name:
            raise ValueError(f"{group_name} contains an empty field name")
        if field.name in seen:
            raise ValueError(
                f"{group_name} contains duplicate field {field.name}"
            )
        if field.value_kind not in valid_kinds:
            raise ValueError(
                f"{group_name} field {field.name} has invalid value kind "
                f"{field.value_kind}"
            )
        if field.source_name is not None and not field.source_name:
            raise ValueError(
                f"{group_name} field {field.name} has an empty source name"
            )
        seen.add(field.name)


def _validate_domain_field_views(
    group_name: str,
    fields: Sequence[DomainFieldSpec],
    views: Mapping[str, Sequence[DomainFieldSpec]],
) -> None:
    expected_names = {field.name for field in fields}
    projected_names: List[str] = []
    for view_name, view_fields in views.items():
        if not view_name:
            raise ValueError(f"{group_name} contains an empty view name")
        if not view_fields:
            raise ValueError(f"{group_name} view {view_name} is empty")
        _validate_domain_field_specs(
            f"{group_name}.{view_name}", view_fields
        )
        projected_names.extend(field.name for field in view_fields)
    if len(projected_names) != len(set(projected_names)):
        raise ValueError(f"{group_name} views overlap")
    if set(projected_names) != expected_names:
        raise ValueError(f"{group_name} views do not cover the canonical fields")


for _group_name, _group_fields in (
    (
        "catalog_flow_consumer_field_coverage",
        CATALOG_FLOW_CONSUMER_FIELD_COVERAGE_FIELDS,
    ),
    (
        "catalog_flow_governance_summary",
        CATALOG_FLOW_GOVERNANCE_SUMMARY_FIELDS,
    ),
    (
        "catalog_flow_filter_governance",
        CATALOG_FLOW_FILTER_GOVERNANCE_FIELDS,
    ),
    (
        "catalog_flow_release_scorecard",
        CATALOG_FLOW_RELEASE_SCORECARD_FIELDS,
    ),
    ("native_core_audit_core", NATIVE_CORE_AUDIT_CORE_FIELDS),
    (
        "native_core_audit_review_manifest",
        NATIVE_CORE_AUDIT_REVIEW_MANIFEST_FIELDS,
    ),
    (
        "native_core_audit_review_coverage",
        NATIVE_CORE_AUDIT_REVIEW_COVERAGE_FIELDS,
    ),
    (
        "final_report_evidence_query_native_audit_scorecard",
        FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_all_repair_path_scorecard",
        RELEASE_PIPELINE_ALL_REPAIR_PATH_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_linkage_scorecard",
        RELEASE_PIPELINE_LINKAGE_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_blocker_drilldown_scorecard",
        RELEASE_PIPELINE_BLOCKER_DRILLDOWN_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_readiness_path_count_scorecard",
        RELEASE_PIPELINE_READINESS_PATH_COUNT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_source_exactness_count_scorecard",
        RELEASE_PIPELINE_SOURCE_EXACTNESS_COUNT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_repair_card_navigation_count_scorecard",
        RELEASE_PIPELINE_REPAIR_CARD_NAVIGATION_COUNT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_source_location_count_scorecard",
        RELEASE_PIPELINE_SOURCE_LOCATION_COUNT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_repair_guidance_count_scorecard",
        RELEASE_PIPELINE_REPAIR_GUIDANCE_COUNT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_catalog_fallback_removal_count_scorecard",
        RELEASE_PIPELINE_CATALOG_FALLBACK_REMOVAL_COUNT_SCORECARD_FIELDS,
    ),
    (
        "final_report_catalog_flow_query_scorecard",
        FINAL_REPORT_CATALOG_FLOW_QUERY_SCORECARD_FIELDS,
    ),
    (
        "final_report_catalog_flow_root_scorecard",
        FINAL_REPORT_CATALOG_FLOW_ROOT_SCORECARD_FIELDS,
    ),
    (
        "final_report_root_cause_catalog_flow_count_scorecard",
        FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_COUNT_SCORECARD_FIELDS,
    ),
    (
        "final_report_root_cause_catalog_flow_state_scorecard",
        FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_STATE_SCORECARD_FIELDS,
    ),
    (
        "final_report_evidence_query_catalog_flow_count_scorecard",
        FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_COUNT_SCORECARD_FIELDS,
    ),
    (
        "final_report_evidence_query_catalog_flow_state_scorecard",
        FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_STATE_SCORECARD_FIELDS,
    ),
    (
        "final_report_evidence_query_ux_selector_scorecard",
        FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_SCORECARD_FIELDS,
    ),
    (
        "final_report_evidence_query_ux_operation_snapshot_scorecard",
        FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_SCORECARD_FIELDS,
    ),
    (
        "final_report_evidence_query_ux_review_scorecard",
        FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_selector_replay_count_scorecard",
        RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_native_boolean_signal_scorecard",
        RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_numeric_boolean_signal_scorecard",
        RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
    ),
    (
        "release_pipeline_cross_artifact_count_scorecard",
        RELEASE_PIPELINE_CROSS_ARTIFACT_COUNT_SCORECARD_FIELDS,
    ),
):
    _validate_domain_field_specs(_group_name, _group_fields)

_validate_domain_field_views(
    "release_pipeline_selector_replay_count_scorecard",
    RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_FIELDS,
    RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_VIEWS,
)

_validate_domain_field_views(
    "release_pipeline_native_boolean_signal_scorecard",
    RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
    RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_VIEWS,
)

_validate_domain_field_views(
    "release_pipeline_numeric_boolean_signal_scorecard",
    RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_FIELDS,
    RELEASE_PIPELINE_NUMERIC_BOOLEAN_SIGNAL_SCORECARD_VIEWS,
)


def domain_metric_pairs(
    data: Dict[str, Any],
    fields: Sequence[DomainFieldSpec],
    source_path: Sequence[PathItem] = (),
    output_prefix: str = "",
) -> List[Tuple[str, Any]]:
    return [
        (
            f"{output_prefix}{field.name}",
            get_path(
                data,
                (
                    *source_path,
                    field.name,
                ),
            ),
        )
        for field in fields
    ]


def summary_domain_metric_pairs(
    data: Dict[str, Any],
    fields: Sequence[DomainFieldSpec],
    prefix: str = "",
) -> List[Tuple[str, Any]]:
    return domain_metric_pairs(
        data,
        fields,
        source_path=("summary",),
        output_prefix=prefix,
    )


def project_domain_fields(
    data: Dict[str, Any],
    fields: Sequence[DomainFieldSpec],
    source_prefix: str = "",
    output_prefix: str = "",
    count_coercer: Optional[Callable[[Any], Any]] = None,
    truthy_coercer: Optional[Callable[[Any], Any]] = None,
) -> Dict[str, Any]:
    projected: Dict[str, Any] = {}
    for field in fields:
        source_name = field.source_name or field.name
        value = data.get(f"{source_prefix}{source_name}", field.default)
        if field.value_kind == DOMAIN_VALUE_COUNT and count_coercer is not None:
            value = count_coercer(value)
        elif (
            field.value_kind == DOMAIN_VALUE_TRUTHY
            and truthy_coercer is not None
        ):
            value = truthy_coercer(value)
        projected[f"{output_prefix}{field.name}"] = value
    return projected


@dataclass(frozen=True)
class FinalReportCatalogFlowFallbackScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_catalog_flow_fallback_scorecard(
    release_pipeline: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportCatalogFlowFallbackScorecardProjection:
    """Project canonical Catalog Flow fallback-removal scorecard surfaces."""
    release = dict(_mapping_value(release_pipeline))
    query_parity_prefix = (
        "evidence_query_catalog_flow_fallback_removal_parity_gate_"
    )
    release[f"{query_parity_prefix}blocking_surface_count"] = _first_non_none(
        release.get(f"{query_parity_prefix}blocking_surface_count"),
        release.get(f"{query_parity_prefix}blocker_count"),
    )
    release[
        f"{query_parity_prefix}blocking_surface_source_location_gap_count"
    ] = _first_non_none(
        release.get(
            f"{query_parity_prefix}blocking_surface_source_location_gap_count"
        ),
        release.get(f"{query_parity_prefix}source_location_gap_count"),
    )
    root_parity_prefix = (
        "root_cause_catalog_flow_fallback_removal_parity_gate_"
    )
    release[
        f"{root_parity_prefix}blocking_surface_source_location_gap_count"
    ] = _first_non_none(
        release.get(
            f"{root_parity_prefix}blocking_surface_source_location_gap_count"
        ),
        release.get(f"{root_parity_prefix}source_location_gap_count"),
    )

    scorecard_fields: Dict[str, Any] = {}
    for fields in (
        RELEASE_PIPELINE_CATALOG_FALLBACK_REMOVAL_COUNT_SCORECARD_FIELDS,
        FINAL_REPORT_CATALOG_FLOW_QUERY_SCORECARD_FIELDS,
        FINAL_REPORT_CATALOG_FLOW_ROOT_SCORECARD_FIELDS,
    ):
        scorecard_fields.update(
            project_domain_fields(
                release,
                fields,
                count_coercer=count_coercer,
                truthy_coercer=truthy_coercer,
            )
        )

    def scorecard_count(name: str) -> int:
        return count_coercer(scorecard_fields.get(name))

    blocker_counts = {
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_root_causes": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_steps": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_step_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gaps": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count"
        ),
        "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_paths": count_coercer(
            release.get(
                "catalog_flow_fallback_removal_shortest_path_repair_path_count"
            )
        ),
        "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gaps": count_coercer(
            release.get(
                "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count"
            )
        ),
        "evidence_query_catalog_flow_fallback_removal_parity_gate_blockers": scorecard_count(
            "release_pipeline_evidence_query_catalog_flow_fallback_removal_parity_gate_blocker_count"
        ),
        "evidence_query_catalog_flow_fallback_removal_parity_gate_parity_refresh_required": scorecard_count(
            "release_pipeline_evidence_query_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count"
        ),
        "evidence_query_catalog_flow_fallback_removal_parity_gate_source_location_gaps": scorecard_count(
            "release_pipeline_evidence_query_catalog_flow_fallback_removal_parity_gate_source_location_gap_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_root_causes": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_blockers": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gaps": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count"
        ),
        "release_pipeline_catalog_flow_fallback_removal_parity_gate_repair_paths": count_coercer(
            release.get(
                "catalog_flow_fallback_removal_parity_gate_repair_path_count"
            )
        ),
        "release_pipeline_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gaps": count_coercer(
            release.get(
                "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count"
            )
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_paths": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_count"
        ),
        "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gaps": scorecard_count(
            "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count"
        ),
    }
    return FinalReportCatalogFlowFallbackScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


@dataclass(frozen=True)
class FinalReportReleasePipelineProductReplayScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_release_pipeline_product_replay_scorecard(
    projection: ReleasePipelineProductReplayProjection,
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportReleasePipelineProductReplayScorecardProjection:
    """Compose Product Replay scorecard and blocker aliases once."""
    values = vars(projection)
    scorecard_fields: Dict[str, Any] = {}

    for fields in (
        RELEASE_PIPELINE_CROSS_ARTIFACT_PRODUCT_REPLAY_COUNT_SCORECARD_FIELDS,
        RELEASE_PIPELINE_PRODUCT_REPLAY_DIRECT_REPAIR_PATH_SCORECARD_FIELDS,
    ):
        scorecard_fields.update(
            project_domain_fields(
                values,
                fields,
                count_coercer=count_coercer,
                truthy_coercer=truthy_coercer,
            )
        )

    selector_values = vars(
        ReleasePipelineSelectorReplayCountScorecardValues(
            evidence_query_product_replay_semantic_linkage_blocker_count=(
                projection.evidence_query_product_replay_semantic_linkage_blocker_count
            ),
            evidence_query_product_replay_semantic_linkage_pending_event_count=(
                projection.evidence_query_product_replay_semantic_linkage_pending_event_count
            ),
            evidence_query_product_replay_semantic_linkage_correlation_count=(
                projection.evidence_query_product_replay_semantic_linkage_correlation_count
            ),
            evidence_query_product_replay_semantic_linkage_target_or_validation_action_count=(
                projection.evidence_query_product_replay_semantic_linkage_target_or_validation_action_count
            ),
        )
    )
    for view_name in (
        "product_replay_blocker",
        "product_replay_pending_event",
        "product_replay_correlation",
        "product_replay_target_or_validation_action",
    ):
        scorecard_fields.update(
            project_domain_fields(
                selector_values,
                RELEASE_PIPELINE_SELECTOR_REPLAY_COUNT_SCORECARD_VIEWS[
                    view_name
                ],
            )
        )

    boolean_values = vars(
        ReleasePipelineNativeBooleanSignalScorecardValues(
            evidence_query_cross_artifact_product_replay_family_present=(
                projection.evidence_query_cross_artifact_product_replay_family_present
            ),
            evidence_query_product_replay_semantic_linkage_sample_replay_present=(
                projection.evidence_query_product_replay_semantic_linkage_sample_replay_present
            ),
        )
    )
    for view_name in (
        "cross_product_replay_family_present",
        "product_replay_sample_present",
    ):
        scorecard_fields.update(
            project_domain_fields(
                boolean_values,
                RELEASE_PIPELINE_NATIVE_BOOLEAN_SIGNAL_SCORECARD_VIEWS[
                    view_name
                ],
                truthy_coercer=truthy_coercer,
            )
        )

    scorecard_fields.update(
        {
            "release_pipeline_evidence_query_cross_artifact_product_replay_family_status": (
                projection.evidence_query_cross_artifact_product_replay_family_status
            ),
            "release_pipeline_evidence_query_cross_artifact_product_replay_family_first_source_artifact": (
                projection.evidence_query_cross_artifact_product_replay_family_first_source_artifact
            ),
            "release_pipeline_evidence_query_cross_artifact_product_replay_family_next_action": (
                projection.evidence_query_cross_artifact_product_replay_family_next_action
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_present": (
                projection.evidence_query_product_replay_semantic_linkage_present
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_status": (
                projection.evidence_query_product_replay_semantic_linkage_status
            ),
            "release_pipeline_evidence_query_product_replay_semantic_events_present": (
                projection.evidence_query_product_replay_semantic_events_present
            ),
            "release_pipeline_evidence_query_product_replay_alignment_present": (
                projection.evidence_query_product_replay_alignment_present
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_read_issue_count": (
                projection.evidence_query_product_replay_semantic_linkage_read_issue_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_sample_event_count": (
                projection.evidence_query_product_replay_semantic_linkage_sample_event_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_replay_supported_event_count": (
                projection.evidence_query_product_replay_semantic_linkage_replay_supported_event_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_aligned_rule_count": (
                projection.evidence_query_product_replay_semantic_linkage_aligned_rule_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_pending_rule_count": (
                projection.evidence_query_product_replay_semantic_linkage_pending_rule_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_ready_correlation_count": (
                projection.evidence_query_product_replay_semantic_linkage_ready_correlation_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_blocked_correlation_count": (
                projection.evidence_query_product_replay_semantic_linkage_blocked_correlation_count
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_first_blocker": (
                projection.evidence_query_product_replay_semantic_linkage_first_blocker
            ),
            "release_pipeline_evidence_query_product_replay_semantic_linkage_next_action": (
                projection.evidence_query_product_replay_semantic_linkage_next_action
            ),
        }
    )

    blocker_counts = {
        "release_pipeline_evidence_query_product_replay_semantic_linkage_blockers": (
            projection.evidence_query_product_replay_semantic_linkage_blocker_count
        ),
        "release_pipeline_evidence_query_product_replay_semantic_linkage_read_issues": (
            projection.evidence_query_product_replay_semantic_linkage_read_issue_count
        ),
        "release_pipeline_evidence_query_product_replay_semantic_linkage_pending_events": (
            projection.evidence_query_product_replay_semantic_linkage_pending_event_count
        ),
        "release_pipeline_evidence_query_product_replay_semantic_linkage_blocked_correlations": (
            projection.evidence_query_product_replay_semantic_linkage_blocked_correlation_count
        ),
        "release_pipeline_evidence_query_product_replay_semantic_linkage_target_or_validation_actions": (
            projection.evidence_query_product_replay_semantic_linkage_target_or_validation_action_count
        ),
        "release_pipeline_product_replay_semantic_linkage_repair_path_count": (
            projection.product_replay_semantic_linkage_repair_path_count
        ),
        "release_pipeline_product_replay_semantic_linkage_repair_path_source_location_gap_count": (
            projection.product_replay_semantic_linkage_repair_path_source_location_gap_count
        ),
        "release_pipeline_evidence_query_product_replay_semantic_linkage_repair_path_count": (
            projection.evidence_query_product_replay_semantic_linkage_repair_path_count
        ),
        "release_pipeline_evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count": (
            projection.evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count
        ),
    }
    return FinalReportReleasePipelineProductReplayScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


@dataclass(frozen=True)
class FinalReportReleasePipelineRepairPathScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_release_pipeline_repair_path_scorecard(
    release_pipeline: Mapping[str, Any],
    cross_artifact: FinalReportEvidenceQueryCrossArtifactProjection,
    product_replay: ReleasePipelineProductReplayProjection,
    root_cause_product_replay: RootCauseProductReplayProjection,
    *,
    count_coercer: Callable[[Any], int],
) -> FinalReportReleasePipelineRepairPathScorecardProjection:
    """Compose Release Pipeline repair-path scorecard surfaces once."""
    release = _mapping_value(release_pipeline)

    def count_value(name: str) -> int:
        return count_coercer(release.get(name))

    aggregate_values = vars(
        ReleasePipelineRepairPathScorecardValues(
            repair_path_source_location_gap_count=count_value(
                "repair_path_source_location_gap_count"
            ),
            repair_path_evidence_counter_count=count_value(
                "repair_path_evidence_counter_count"
            ),
            repair_path_group_count=count_value("repair_path_group_count"),
            repair_path_environment_group_count=count_value(
                "repair_path_environment_group_count"
            ),
            repair_path_category_group_count=count_value(
                "repair_path_category_group_count"
            ),
            repair_path_current_repo_action_group_count=count_value(
                "repair_path_current_repo_action_group_count"
            ),
            repair_path_target_or_validation_action_group_count=count_value(
                "repair_path_target_or_validation_action_group_count"
            ),
            repair_path_group_plan_step_count=count_value(
                "repair_path_group_plan_step_count"
            ),
            repair_path_group_plan_current_repo_step_count=count_value(
                "repair_path_group_plan_current_repo_step_count"
            ),
            repair_path_group_plan_target_or_validation_step_count=count_value(
                "repair_path_group_plan_target_or_validation_step_count"
            ),
            catalog_flow_repair_path_source_location_gap_count=count_value(
                "catalog_flow_repair_path_source_location_gap_count"
            ),
            catalog_flow_repair_path_evidence_counter_count=count_value(
                "catalog_flow_repair_path_evidence_counter_count"
            ),
            evidence_query_snapshot_diff_repair_path_source_location_gap_count=count_value(
                "evidence_query_snapshot_diff_repair_path_source_location_gap_count"
            ),
            evidence_query_snapshot_diff_repair_path_evidence_counter_count=count_value(
                "evidence_query_snapshot_diff_repair_path_evidence_counter_count"
            ),
            native_core_audit_repair_path_evidence_counter_count=count_value(
                "native_core_audit_repair_path_evidence_counter_count"
            ),
            native_core_audit_scope_repair_path_evidence_counter_count=count_value(
                "native_core_audit_scope_repair_path_evidence_counter_count"
            ),
            evidence_query_cross_artifact_release_focus_repair_path_count=(
                cross_artifact.release_focus_repair_path_count
            ),
            evidence_query_product_replay_semantic_linkage_repair_path_count=(
                product_replay.evidence_query_product_replay_semantic_linkage_repair_path_count
            ),
            evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count=(
                product_replay.evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count
            ),
            root_cause_product_replay_semantic_linkage_repair_path_count=(
                root_cause_product_replay.product_replay_semantic_linkage_repair_path_count
            ),
            root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count=(
                root_cause_product_replay.product_replay_semantic_linkage_repair_path_source_location_gap_count
            ),
            root_cause_product_replay_family_gate_repair_path_count=(
                root_cause_product_replay.product_replay_family_gate_repair_path_count
            ),
            root_cause_product_replay_family_gate_repair_path_source_location_gap_count=(
                root_cause_product_replay.product_replay_family_gate_repair_path_source_location_gap_count
            ),
        )
    )
    scorecard_fields: Dict[str, Any] = {}
    for fields in (
        RELEASE_PIPELINE_CATALOG_FLOW_DIRECT_REPAIR_PATH_SCORECARD_FIELDS,
        RELEASE_PIPELINE_REPORT_DIRECT_REPAIR_PATH_SCORECARD_FIELDS,
        RELEASE_PIPELINE_NATIVE_AUDIT_DIRECT_REPAIR_PATH_SCORECARD_FIELDS,
    ):
        scorecard_fields.update(
            project_domain_fields(
                dict(release),
                fields,
                count_coercer=count_coercer,
            )
        )
    scorecard_fields.update(
        project_domain_fields(
            aggregate_values,
            RELEASE_PIPELINE_REPAIR_PATH_SCORECARD_FIELDS,
        )
    )
    scorecard_fields.update(
        {
            "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count": count_value(
                "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count"
            ),
            "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count": count_value(
                "catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count"
            ),
            "release_pipeline_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_count": count_value(
                "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_count"
            ),
            "release_pipeline_catalog_flow_fallback_removal_parity_gate_repair_path_evidence_counter_count": count_value(
                "catalog_flow_fallback_removal_parity_gate_repair_path_evidence_counter_count"
            ),
            "release_pipeline_native_core_audit_scope_repair_path_source_location_gap_count": count_value(
                "native_core_audit_scope_repair_path_source_location_gap_count"
            ),
        }
    )

    blocker_counts = {
        "repair_paths": count_value("repair_path_count"),
        "release_pipeline_repair_path_source_location_gaps": count_value(
            "repair_path_source_location_gap_count"
        ),
        "release_pipeline_catalog_flow_repair_paths": count_value(
            "catalog_flow_repair_path_count"
        ),
        "release_pipeline_catalog_flow_source_location_gaps": count_value(
            "catalog_flow_repair_path_source_location_gap_count"
        ),
        "release_pipeline_evidence_query_snapshot_diff_repair_paths": count_value(
            "evidence_query_snapshot_diff_repair_path_count"
        ),
        "release_pipeline_evidence_query_snapshot_diff_source_location_gaps": count_value(
            "evidence_query_snapshot_diff_repair_path_source_location_gap_count"
        ),
        "release_pipeline_native_core_audit_repair_paths": count_value(
            "native_core_audit_repair_path_count"
        ),
        "release_pipeline_native_core_audit_source_location_gaps": count_value(
            "native_core_audit_repair_path_source_location_gap_count"
        ),
    }
    return FinalReportReleasePipelineRepairPathScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


@dataclass(frozen=True)
class FinalReportRootCauseCatalogFlowScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_root_cause_catalog_flow_scorecard(
    root_cause_review: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportRootCauseCatalogFlowScorecardProjection:
    """Project the canonical Root Cause Catalog Flow review for Final Report."""
    review = dict(_mapping_value(root_cause_review))
    parity_prefix = "catalog_flow_fallback_removal_parity_gate_"
    review[f"{parity_prefix}blocking_surface_count"] = _first_non_none(
        review.get(f"{parity_prefix}blocking_surface_count"),
        review.get(f"{parity_prefix}blocker_count"),
    )
    review[
        f"{parity_prefix}blocking_surface_source_location_gap_count"
    ] = _first_non_none(
        review.get(
            f"{parity_prefix}blocking_surface_source_location_gap_count"
        ),
        review.get(f"{parity_prefix}source_location_gap_count"),
    )

    scorecard_fields: Dict[str, Any] = {}
    for fields in (
        FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_COUNT_SCORECARD_FIELDS,
        FINAL_REPORT_ROOT_CAUSE_CATALOG_FLOW_STATE_SCORECARD_FIELDS,
    ):
        scorecard_fields.update(
            project_domain_fields(
                review,
                fields,
                count_coercer=count_coercer,
                truthy_coercer=truthy_coercer,
            )
        )

    action_source_count = count_coercer(
        _first_non_none(
            review.get("catalog_flow_repair_card_count"),
            review.get("catalog_flow_root_cause_count"),
        )
    )
    action_source_location_gap_count = max(
        action_source_count
        - count_coercer(review.get("catalog_flow_source_location_card_count")),
        0,
    )
    scorecard_fields.update(
        {
            "root_cause_catalog_flow_action_source_count": action_source_count,
            "root_cause_catalog_flow_action_source_location_gap_count": (
                action_source_location_gap_count
            ),
        }
    )

    def scorecard_count(name: str) -> int:
        return count_coercer(scorecard_fields.get(name))

    blocker_counts = {
        "root_cause_repair_cards": count_coercer(
            review.get("repair_card_count")
        ),
        "root_cause_catalog_flow_root_causes": scorecard_count(
            "root_cause_catalog_flow_root_cause_count"
        ),
        "root_cause_catalog_flow_surface_parity_refresh_required": scorecard_count(
            "root_cause_catalog_flow_surface_parity_refresh_required_count"
        ),
        "root_cause_catalog_flow_fallback_removal_shortest_path_root_causes": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count"
        ),
        "root_cause_catalog_flow_fallback_removal_shortest_path_steps": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_shortest_path_step_count"
        ),
        "root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gaps": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count"
        ),
        "root_cause_catalog_flow_fallback_removal_parity_gate_root_causes": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count"
        ),
        "root_cause_catalog_flow_fallback_removal_parity_gate_blockers": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count"
        ),
        "root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count"
        ),
        "root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gaps": scorecard_count(
            "root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count"
        ),
        "root_cause_catalog_flow_source_location_gaps": (
            action_source_location_gap_count
        ),
    }
    return FinalReportRootCauseCatalogFlowScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


@dataclass(frozen=True)
class FinalReportEvidenceQueryCatalogFlowScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_evidence_query_catalog_flow_scorecard(
    catalog_flow_filter: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportEvidenceQueryCatalogFlowScorecardProjection:
    """Project the canonical Evidence Query Catalog Flow filter."""
    filter_view = dict(_mapping_value(catalog_flow_filter))
    scorecard_fields: Dict[str, Any] = {}
    for fields in (
        FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_COUNT_SCORECARD_FIELDS,
        FINAL_REPORT_EVIDENCE_QUERY_CATALOG_FLOW_STATE_SCORECARD_FIELDS,
    ):
        scorecard_fields.update(
            project_domain_fields(
                filter_view,
                fields,
                count_coercer=count_coercer,
                truthy_coercer=truthy_coercer,
            )
        )
    blocker_counts = {
        "evidence_query_catalog_flow_surface_parity_refresh_required": (
            count_coercer(
                scorecard_fields.get(
                    "evidence_query_catalog_flow_surface_parity_refresh_required_count"
                )
            )
        ),
    }
    return FinalReportEvidenceQueryCatalogFlowScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


@dataclass(frozen=True)
class FinalReportEvidenceQueryNativeAuditScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


@dataclass(frozen=True)
class FinalReportRootCauseNativeAuditResidualProjection:
    scorecard_fields: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, int]:
        return dict(self.scorecard_fields)


def project_final_report_root_cause_native_audit_residual(
    root_cause_review: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
) -> FinalReportRootCauseNativeAuditResidualProjection:
    """Project legacy Native Audit root-cause counters once for the report."""
    review = _mapping_value(root_cause_review)
    root_cause_count = count_coercer(
        _first_non_none(
            review.get("native_core_audit_root_cause_count"),
            review.get("native_core_audit_backlog_root_cause_count"),
        )
    )
    return FinalReportRootCauseNativeAuditResidualProjection(
        scorecard_fields={
            "root_cause_native_core_audit_root_cause_count": root_cause_count,
            "root_cause_native_core_audit_backlog_root_cause_count": count_coercer(
                review.get("native_core_audit_backlog_root_cause_count")
            ),
            "root_cause_native_core_audit_scope_root_cause_count": count_coercer(
                review.get("native_core_audit_scope_root_cause_count")
            ),
            "root_cause_native_core_audit_blocker_count": count_coercer(
                review.get("native_core_audit_blocker_count")
            ),
            "root_cause_native_core_audit_source_unreadable_count": count_coercer(
                review.get("native_core_audit_source_unreadable_count")
            ),
        }
    )


def project_final_report_evidence_query_native_audit_scorecard(
    native_audit_filter: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportEvidenceQueryNativeAuditScorecardProjection:
    """Project the bounded Evidence Query Native Audit scorecard surface."""
    source = dict(_mapping_value(native_audit_filter))

    scorecard_fields = project_domain_fields(
        source,
        FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_SCORECARD_FIELDS,
        count_coercer=count_coercer,
        truthy_coercer=truthy_coercer,
    )
    blocker_counts = {
        alias: count_coercer(
            scorecard_fields.get(
                f"evidence_query_native_core_audit_{source_name}"
            )
        )
        for alias, source_name in (
            _FINAL_REPORT_EVIDENCE_QUERY_NATIVE_AUDIT_BLOCKER_ALIAS_SOURCES
        )
    }
    return FinalReportEvidenceQueryNativeAuditScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


def project_final_report_evidence_query_ux_selector_scorecard(
    evidence_query_ux: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
) -> Dict[str, Any]:
    """Project the bounded Evidence Query selector UX scorecard surface."""
    return project_domain_fields(
        _mapping_value(evidence_query_ux),
        FINAL_REPORT_EVIDENCE_QUERY_UX_SELECTOR_SCORECARD_FIELDS,
        count_coercer=count_coercer,
    )


def project_final_report_evidence_query_ux_operation_snapshot_scorecard(
    evidence_query_ux: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
) -> Dict[str, Any]:
    """Project bounded Evidence Query operation and snapshot UX fields."""
    return project_domain_fields(
        _mapping_value(evidence_query_ux),
        FINAL_REPORT_EVIDENCE_QUERY_UX_OPERATION_SNAPSHOT_SCORECARD_FIELDS,
        count_coercer=count_coercer,
    )


@dataclass(frozen=True)
class FinalReportEvidenceQueryUxReviewScorecardProjection:
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_evidence_query_ux_review_scorecard(
    evidence_query_ux: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportEvidenceQueryUxReviewScorecardProjection:
    """Project bounded Evidence Query repair/source/drilldown UX fields."""
    source = dict(_mapping_value(evidence_query_ux))
    for source_name in (
        _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_NUMERIC_BOOLEAN_SOURCE_NAMES
    ):
        source[source_name] = 1 if truthy_coercer(source.get(source_name)) else 0

    scorecard_fields = project_domain_fields(
        source,
        FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_SCORECARD_FIELDS,
        count_coercer=count_coercer,
    )
    blocker_counts = {
        alias: count_coercer(
            scorecard_fields.get(f"evidence_query_ux_{source_name}")
        )
        for alias, source_name in (
            _FINAL_REPORT_EVIDENCE_QUERY_UX_REVIEW_BLOCKER_ALIAS_SOURCES
        )
    }
    return FinalReportEvidenceQueryUxReviewScorecardProjection(
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


@dataclass(frozen=True)
class FinalReportCatalogFlowFallbackReviewProjection:
    queue: Mapping[str, Any]
    shortest_path: Mapping[str, Any]
    parity_gate: Mapping[str, Any]
    scorecard_fields: Mapping[str, Any]
    blocker_counts: Mapping[str, int]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)

    def to_blocker_counts_dict(self) -> Dict[str, int]:
        return dict(self.blocker_counts)


def project_final_report_catalog_flow_fallback_review(
    fallback_removal_queue: Mapping[str, Any],
    shortest_path: Mapping[str, Any],
    parity_gate: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], int],
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportCatalogFlowFallbackReviewProjection:
    """Normalize one request's Catalog Flow fallback-removal review state."""
    queue_source = _mapping_value(fallback_removal_queue)
    shortest_source = _mapping_value(shortest_path)
    parity_source = _mapping_value(parity_gate)

    queue = {
        "item_count": count_coercer(
            queue_source.get("catalog_flow_fallback_removal_queue_item_count")
        ),
        "blocked_count": count_coercer(
            queue_source.get("catalog_flow_fallback_removal_queue_blocked_count")
        ),
        "parity_refresh_required_count": count_coercer(
            queue_source.get(
                "catalog_flow_fallback_removal_queue_parity_refresh_required_count"
            )
        ),
        "current_repo_action_count": count_coercer(
            queue_source.get(
                "catalog_flow_fallback_removal_queue_current_repo_action_count"
            )
        ),
        "target_machine_action_count": count_coercer(
            queue_source.get(
                "catalog_flow_fallback_removal_queue_target_machine_action_count"
            )
        ),
        "canonical_source": queue_source.get("canonical_source"),
        "canonical_gap_count": count_coercer(
            queue_source.get("canonical_gap_count")
        ),
        "source_drift_count": count_coercer(
            queue_source.get("source_drift_count")
        ),
        "source_drift_fields": queue_source.get("source_drift_fields", []),
        "source_drift_sources": queue_source.get("source_drift_sources", []),
    }
    queue["repair_action_required"] = any(
        count_coercer(queue[name]) > 0
        for name in (
            "blocked_count",
            "parity_refresh_required_count",
            "current_repo_action_count",
            "target_machine_action_count",
            "canonical_gap_count",
            "source_drift_count",
        )
    )
    queue["trace_present"] = count_coercer(queue["canonical_gap_count"]) == 0
    queue["trace_action_count"] = max(
        count_coercer(queue[name])
        for name in (
            "blocked_count",
            "parity_refresh_required_count",
            "current_repo_action_count",
            "target_machine_action_count",
            "canonical_gap_count",
            "source_drift_count",
        )
    )
    queue["repair_environment"] = (
        "current_repo_fixable"
        if count_coercer(queue["canonical_gap_count"]) > 0
        or count_coercer(queue["source_drift_count"]) > 0
        or count_coercer(queue["current_repo_action_count"]) > 0
        else "target_machine_or_validation"
    )
    queue["action_artifact"] = _first_non_none(
        queue["canonical_source"],
        "native_release_evidence_pipeline.json",
    )

    shortest = {
        "present": truthy_coercer(shortest_source.get("present")),
        "state": _first_non_none(shortest_source.get("state"), "not_reported"),
        "step_count": count_coercer(shortest_source.get("step_count")),
        "current_repo_step_count": count_coercer(
            shortest_source.get("current_repo_step_count")
        ),
        "target_machine_step_count": count_coercer(
            shortest_source.get("target_machine_step_count")
        ),
        "source_location_gap_count": count_coercer(
            shortest_source.get("source_location_gap_count")
        ),
        "primary_environment": _first_non_none(
            shortest_source.get("primary_environment"),
            "not_applicable",
        ),
        "first_surface": _first_non_none(
            shortest_source.get("first_surface"),
            "",
        ),
        "repair_first_surface": _first_non_none(
            shortest_source.get("first_surface"),
            "unknown",
        ),
        "canonical_source": shortest_source.get("canonical_source"),
        "canonical_gap_count": count_coercer(
            shortest_source.get("canonical_gap_count")
        ),
        "source_drift_count": count_coercer(
            shortest_source.get("source_drift_count")
        ),
        "source_drift_fields": list_value(
            shortest_source.get("source_drift_fields")
        ),
        "source_drift_sources": list_value(
            shortest_source.get("source_drift_sources")
        ),
        "canonical_ready": truthy_coercer(
            shortest_source.get("canonical_ready")
        ),
        "source_consistent": truthy_coercer(
            shortest_source.get("source_consistent")
        ),
    }
    shortest["blocks_removal"] = (
        truthy_coercer(shortest_source.get("blocks_removal"))
        or count_coercer(shortest["canonical_gap_count"]) > 0
        or count_coercer(shortest["source_drift_count"]) > 0
    )
    shortest["repair_action_required"] = bool(shortest["blocks_removal"]) or any(
        count_coercer(shortest[name]) > 0
        for name in (
            "step_count",
            "current_repo_step_count",
            "target_machine_step_count",
            "source_location_gap_count",
        )
    )
    shortest["repair_environment"] = (
        "current_repo_fixable"
        if count_coercer(shortest["canonical_gap_count"]) > 0
        or count_coercer(shortest["source_drift_count"]) > 0
        else shortest["primary_environment"]
    )

    parity = {
        "present": truthy_coercer(parity_source.get("present")),
        "decision": _first_non_none(
            parity_source.get("decision"),
            "not_reported",
        ),
        "repair_action_decision": parity_source.get("decision"),
        "removal_allowed": truthy_coercer(
            parity_source.get("removal_allowed")
        ),
        "repair_action_removal_allowed": parity_source.get("removal_allowed"),
        "blocker_count": count_coercer(parity_source.get("blocker_count")),
        "fallback_surface_count": count_coercer(
            parity_source.get("fallback_surface_count")
        ),
        "parity_refresh_required_count": count_coercer(
            parity_source.get("parity_refresh_required_count")
        ),
        "current_repo_action_count": count_coercer(
            parity_source.get("current_repo_action_count")
        ),
        "target_machine_action_count": count_coercer(
            parity_source.get("target_machine_action_count")
        ),
        "source_location_gap_count": count_coercer(
            parity_source.get("source_location_gap_count")
        ),
        "first_surface": _first_non_none(
            parity_source.get("first_surface"),
            "",
        ),
        "canonical_source": parity_source.get("canonical_source"),
        "canonical_gap_count": count_coercer(
            parity_source.get("canonical_gap_count")
        ),
        "source_drift_count": count_coercer(
            parity_source.get("source_drift_count")
        ),
        "source_drift_fields": list_value(
            parity_source.get("source_drift_fields")
        ),
        "source_drift_sources": list_value(
            parity_source.get("source_drift_sources")
        ),
        "canonical_ready": truthy_coercer(
            parity_source.get("canonical_ready")
        ),
        "source_consistent": truthy_coercer(
            parity_source.get("source_consistent")
        ),
    }
    parity["repair_action_required"] = (
        truthy_coercer(parity_source.get("blocks_removal"))
        or count_coercer(parity["blocker_count"]) > 0
        or count_coercer(parity["parity_refresh_required_count"]) > 0
        or count_coercer(parity["source_location_gap_count"]) > 0
        or count_coercer(parity["canonical_gap_count"]) > 0
        or count_coercer(parity["source_drift_count"]) > 0
    )
    parity["blocks_removal"] = (
        bool(parity["repair_action_required"])
        or count_coercer(parity["current_repo_action_count"]) > 0
        or count_coercer(parity["target_machine_action_count"]) > 0
        or (
            bool(parity["present"])
            and not bool(parity["removal_allowed"])
        )
    )
    parity["repair_environment"] = (
        "current_repo_fixable"
        if count_coercer(parity["canonical_gap_count"]) > 0
        or count_coercer(parity["source_drift_count"]) > 0
        else _first_non_none(
            parity_source.get("primary_environment"),
            "target_machine_or_validation",
        )
    )

    scorecard_fields = {
        "catalog_flow_fallback_removal_queue_total_item_count": queue["item_count"],
        "catalog_flow_fallback_removal_queue_total_blocked_count": queue["blocked_count"],
        "catalog_flow_fallback_removal_queue_total_parity_refresh_required_count": queue["parity_refresh_required_count"],
        "catalog_flow_fallback_removal_queue_total_current_repo_action_count": queue["current_repo_action_count"],
        "catalog_flow_fallback_removal_queue_total_target_machine_action_count": queue["target_machine_action_count"],
        "catalog_flow_fallback_removal_queue_canonical_source": queue["canonical_source"],
        "catalog_flow_fallback_removal_queue_canonical_gap_count": queue["canonical_gap_count"],
        "catalog_flow_fallback_removal_queue_source_drift_count": queue["source_drift_count"],
        "catalog_flow_fallback_removal_queue_source_drift_fields": queue["source_drift_fields"],
        "catalog_flow_fallback_removal_queue_source_drift_sources": queue["source_drift_sources"],
        "catalog_flow_fallback_removal_shortest_path_present": shortest["present"],
        "catalog_flow_fallback_removal_shortest_path_state": shortest["state"],
        "catalog_flow_fallback_removal_shortest_path_step_count": shortest["step_count"],
        "catalog_flow_fallback_removal_shortest_path_current_repo_step_count": shortest["current_repo_step_count"],
        "catalog_flow_fallback_removal_shortest_path_target_machine_step_count": shortest["target_machine_step_count"],
        "catalog_flow_fallback_removal_shortest_path_source_location_gap_count": shortest["source_location_gap_count"],
        "catalog_flow_fallback_removal_shortest_path_primary_environment": shortest["primary_environment"],
        "catalog_flow_fallback_removal_shortest_path_first_surface": shortest["first_surface"],
        "catalog_flow_fallback_removal_shortest_path_blocks_removal": shortest["blocks_removal"],
        "catalog_flow_fallback_removal_shortest_path_canonical_source": shortest["canonical_source"],
        "catalog_flow_fallback_removal_shortest_path_canonical_gap_count": shortest["canonical_gap_count"],
        "catalog_flow_fallback_removal_shortest_path_source_drift_count": shortest["source_drift_count"],
        "catalog_flow_fallback_removal_shortest_path_source_drift_fields": shortest["source_drift_fields"],
        "catalog_flow_fallback_removal_shortest_path_source_drift_sources": shortest["source_drift_sources"],
        "catalog_flow_fallback_removal_shortest_path_canonical_ready": shortest["canonical_ready"],
        "catalog_flow_fallback_removal_shortest_path_source_consistent": shortest["source_consistent"],
        "catalog_flow_fallback_removal_parity_gate_present": parity["present"],
        "catalog_flow_fallback_removal_parity_gate_decision": parity["decision"],
        "catalog_flow_fallback_removal_parity_gate_removal_allowed": parity["removal_allowed"],
        "catalog_flow_fallback_removal_parity_gate_total_blocker_count": parity["blocker_count"],
        "catalog_flow_fallback_removal_parity_gate_total_fallback_surface_count": parity["fallback_surface_count"],
        "catalog_flow_fallback_removal_parity_gate_total_parity_refresh_required_count": parity["parity_refresh_required_count"],
        "catalog_flow_fallback_removal_parity_gate_total_current_repo_action_count": parity["current_repo_action_count"],
        "catalog_flow_fallback_removal_parity_gate_total_target_machine_action_count": parity["target_machine_action_count"],
        "catalog_flow_fallback_removal_parity_gate_total_source_location_gap_count": parity["source_location_gap_count"],
        "catalog_flow_fallback_removal_parity_gate_first_surface": parity["first_surface"],
        "catalog_flow_fallback_removal_parity_gate_blocks_removal": parity["blocks_removal"],
        "catalog_flow_fallback_removal_parity_gate_canonical_source": parity["canonical_source"],
        "catalog_flow_fallback_removal_parity_gate_canonical_gap_count": parity["canonical_gap_count"],
        "catalog_flow_fallback_removal_parity_gate_source_drift_count": parity["source_drift_count"],
        "catalog_flow_fallback_removal_parity_gate_source_drift_fields": parity["source_drift_fields"],
        "catalog_flow_fallback_removal_parity_gate_source_drift_sources": parity["source_drift_sources"],
        "catalog_flow_fallback_removal_parity_gate_canonical_ready": parity["canonical_ready"],
        "catalog_flow_fallback_removal_parity_gate_source_consistent": parity["source_consistent"],
    }
    blocker_counts = {
        "catalog_flow_fallback_removal_queue_blocked": queue["blocked_count"],
        "catalog_flow_fallback_removal_queue_parity_refresh_required": queue["parity_refresh_required_count"],
        "catalog_flow_fallback_removal_queue_current_repo_actions": queue["current_repo_action_count"],
        "catalog_flow_fallback_removal_queue_target_machine_actions": queue["target_machine_action_count"],
        "catalog_flow_fallback_removal_queue_canonical_gaps": queue["canonical_gap_count"],
        "catalog_flow_fallback_removal_queue_source_drift": queue["source_drift_count"],
        "catalog_flow_fallback_removal_shortest_path_steps": shortest["step_count"],
        "catalog_flow_fallback_removal_shortest_path_current_repo_steps": shortest["current_repo_step_count"],
        "catalog_flow_fallback_removal_shortest_path_target_machine_steps": shortest["target_machine_step_count"],
        "catalog_flow_fallback_removal_shortest_path_source_location_gaps": shortest["source_location_gap_count"],
    }
    return FinalReportCatalogFlowFallbackReviewProjection(
        queue=queue,
        shortest_path=shortest,
        parity_gate=parity,
        scorecard_fields=scorecard_fields,
        blocker_counts=blocker_counts,
    )


def artifact_catalog_prometheus_metric_pairs(
    catalog_review: Mapping[str, Any],
) -> List[Tuple[str, Any]]:
    return [
        (name, catalog_review.get(name))
        for name in ARTIFACT_CATALOG_PROMETHEUS_METRIC_NAMES
    ]


def raw_mode(data: Dict[str, Any]) -> str:
    mode = data.get("mode")
    if isinstance(mode, str) and mode.strip():
        return mode.strip()
    execution_kind = data.get("execution_kind")
    if isinstance(execution_kind, str) and execution_kind.strip():
        return execution_kind.strip()
    return ""


def mode_matches(data: Dict[str, Any], expected: str) -> bool:
    actual = raw_mode(data)
    if actual == expected:
        return True
    if expected == "dry_run":
        return actual in {"dry_run_plan", "dry_run_ready"}
    if expected == "replay":
        return actual in {"replay_supported", "replay_ready"}
    return False


def claim_scope_present(data: Optional[Dict[str, Any]]) -> bool:
    if not isinstance(data, dict):
        return False
    claim_scope = data.get("claim_scope")
    if isinstance(claim_scope, str):
        return bool(claim_scope.strip())
    if isinstance(claim_scope, (dict, list)):
        return bool(claim_scope)
    claim_scope_detail = data.get("claim_scope_detail")
    return isinstance(claim_scope_detail, dict) and bool(claim_scope_detail)


def artifact_type_name(data: Optional[Dict[str, Any]], source: str = "") -> str:
    if isinstance(data, dict):
        artifact_type = data.get("artifact_type")
        if isinstance(artifact_type, str) and artifact_type.strip():
            return artifact_type.strip()
        schema = data.get("schema")
        if isinstance(schema, dict):
            schema_name = schema.get("name")
            if isinstance(schema_name, str) and schema_name.strip():
                return schema_name.strip()
    return Path(source).stem if source else "unknown"


def projected_mode(data: Optional[Dict[str, Any]]) -> str:
    if not isinstance(data, dict):
        return "missing"
    mode = raw_mode(data)
    if mode:
        return mode
    if data.get("replayed") is True:
        return "replay"
    if data.get("executed") is False:
        return "dry_run"
    return "unknown"


def projected_release_decision(data: Optional[Dict[str, Any]]) -> str:
    if not isinstance(data, dict):
        return "missing"
    summary = data.get("summary")
    candidates = []
    if isinstance(summary, dict):
        candidates.extend(
            (summary.get("release_decision"), summary.get("decision"))
        )
    candidates.extend((data.get("release_decision"), data.get("decision")))
    for candidate in candidates:
        if isinstance(candidate, str) and candidate.strip():
            return candidate.strip()
    return "unknown"


def projected_blocker_count(data: Optional[Dict[str, Any]]) -> Optional[int]:
    if not isinstance(data, dict):
        return None
    summary = data.get("summary")
    candidates = []
    if isinstance(summary, dict):
        candidates.extend(
            (
                summary.get("blocker_count"),
                summary.get("release_blocking_signal_count"),
            )
        )
    candidates.append(data.get("blocker_count"))
    for candidate in candidates:
        if isinstance(candidate, (int, float)) and not isinstance(
            candidate, bool
        ):
            number = float(candidate)
            if math.isfinite(number) and number.is_integer():
                return max(0, int(number))
    blockers = data.get("blockers")
    return len(blockers) if isinstance(blockers, list) else 0


def artifact_evidence_projection(
    data: Optional[Dict[str, Any]], source: str = ""
) -> Dict[str, Any]:
    """Project stable evidence metadata without reimplementing native reasoning."""
    present = isinstance(data, dict)
    status = "missing"
    if present:
        raw_status = data.get("status")
        status = (
            raw_status.strip()
            if isinstance(raw_status, str) and raw_status.strip()
            else "present"
        )
    executed = data.get("executed") if present else None
    replayed = data.get("replayed") if present else None
    return {
        "source": source,
        "present": present,
        "status": status,
        "artifact_type": artifact_type_name(data, source),
        "mode": projected_mode(data),
        "executed": executed if isinstance(executed, bool) else None,
        "replayed": replayed if isinstance(replayed, bool) else None,
        "schema_present": bool(
            present and ("schema" in data or "schema_version" in data)
        ),
        "summary_present": bool(
            present and isinstance(data.get("summary"), dict)
        ),
        "claim_scope_present": claim_scope_present(data),
        "blocker_count": projected_blocker_count(data),
        "release_decision": projected_release_decision(data),
        "confidence": infer_evidence_confidence(data),
    }


def infer_evidence_confidence(data: Optional[Dict[str, Any]]) -> str:
    if not isinstance(data, dict):
        return "stale_or_blocked"
    summary = data.get("summary")
    if isinstance(summary, dict) and isinstance(
        summary.get("evidence_confidence"), str
    ):
        return summary["evidence_confidence"]
    if isinstance(data.get("evidence_confidence"), str):
        return data["evidence_confidence"]
    blockers = data.get("blockers")
    if isinstance(blockers, list) and blockers:
        return "stale_or_blocked"
    if data.get("executed") is True and mode_matches(data, "execute"):
        return "execute_verified"
    if data.get("replayed") is True:
        return "replay_supported"
    has_schema = "schema" in data or "schema_version" in data
    has_claim = "claim_scope" in data or "claim_scope_detail" in data
    if has_schema and has_claim and isinstance(summary, dict):
        return "dry_run_ready"
    if has_schema:
        return "schema_only"
    return "stale_or_blocked"
